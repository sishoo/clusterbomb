#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <float.h>
#include <math.h>
#include <time.h>
#include <string.h>
#include <stdbool.h>

// clang main.c -o main -Ofast -Wall -mavx

// clang -g main.c -o main -O0 -Wall -fsanitize=address

#define CLUSTER_GROUP_TRIANGLE_NUM 128

/*
        have various triangle generalizations
        max res is original mesh
        there are generalizations generated that all have to match each other

        make a relation table

        Faces [ {0 1 2}, {1 4 2}, {1 3 4}]
        Vertices [0, 1, 2, 3, 4]

                         3|
                        / |
                       /  |
                      /   |
                     /    |
                    /   2 |
        0_________1_______4
         \        |      /
          \       | 1   |
           \  0   |    /
            \     |   |
             \    |   |
              \   |   /
               \  |  |
                \ | /
                 \2/


        relation table:
        V       F
        0:  0
        1:  0, 1, 2
        2:  0, 1
        3:  2
        4:  1, 2


        methods of getting groups:
                - uv map + weird distance thing (distance map type of thing?)
                        1) assign each triangle a value by their centers distance from 0, 0;
                        2) generate the uv map
                        3)
                - walk a half edge graph thing

                - bucket sort
                        - based of centroid distance from 0, 0
                        - get aabb type thing and divide it

*/

typedef struct
{
        float x, y, z;
} Vertex, Vec3;

typedef struct
{
        uint32_t v1, v2, v3;
} Face;

typedef struct
{
        uint32_t v1v2, v1v3, v2v3;
} AdjacencyData;

typedef struct
{
        uint32_t cluster_resolution;
        uint32_t face_indices[1];
} Cluster;

typedef struct
{
        void *data; /* For quality of life */

        uint32_t num_clusters;
        Cluster **cluster_heigharchy; /* cluster_heigharchy[0] is base the level mesh, the clusters get chunkier as you go out */

        uint32_t num_faces, num_vertices;
        Vertex *vertices;

        AdjacencyData *adjacency_data;
        Face *faces;
} Mesh;

void triangulate_polygon(Vec3 *const p)
{
}

void mesh_generate_clusters(Mesh *const m)
{
        /*
                Currently using naive k means. use a faster one

                cluster based off vertices

                use vernoi thing as polygons
        */

        uint32_t k = round(m->num_faces / CLUSTER_GROUP_TRIANGLE_NUM);

        Vec3 *means = malloc(sizeof(Vec3) * (k + m->num_faces) + sizeof(uint32_t) * m->num_faces);
        Vec3 *face_centroids = means + k;
        uint32_t *closest_means = face_centroids + m->num_faces;

        char *changed = malloc(sizeof());

        /* Generate off with random means */
        for (uint32_t i = 0; i < k; i++)
        {
                means[i].x = (float)rand() / (float)RAND_MAX;
                means[i].y = (float)rand() / (float)RAND_MAX;
                means[i].z = (float)rand() / (float)RAND_MAX;
        }

        /* Get the faces centroids */
        for (uint32_t i = 0; i < m->num_faces; i++)
        {
                Face face = m->faces[i];
                Vertex v1 = m->vertices[face.v1];
                Vertex v2 = m->vertices[face.v2];
                Vertex v3 = m->vertices[face.v3];

                face_centroids[i].x = (v1.x + v2.x + v3.x) / 3;
                face_centroids[i].y = (v1.y + v2.y + v3.y) / 3;
                face_centroids[i].z = (v1.z + v2.z + v3.z) / 3;
        }

        while (1)
        {
                /* Group the faces with the closest means */
                for (uint32_t i = 0; i < m->num_faces; i++)
                {
                        Vec3 face_centroid = face_centroids[i];
                        float min_dist_squared = FLT_MAX;
                        uint32_t closest_mean;
                        for (uint32_t j = 0; j < k; j++)
                        {
                                Vec3 current_mean = means[i];

                                float x_dist_squared = (face_centroid.x - current_mean.x) * (face_centroid.x - current_mean.x);
                                float y_dist_squared = (face_centroid.y - current_mean.y) * (face_centroid.y - current_mean.y);
                                float z_dist_squared = (face_centroid.z - current_mean.z) * (face_centroid.z - current_mean.z);
                                float current_dist_squared = x_dist_squared + y_dist_squared + z_dist_squared;

                                if (current_dist_squared < min_dist_squared)
                                {
                                        min_dist_squared = current_dist_squared;
                                        closest_mean = j;
                                }
                        }       
                        closest_means[i] = closest_mean;
                }

                for (uint32_t i = 0; i < k; i++)
                {

                }
                
        }

        free(means);
}

void mesh_init_adjacency_data(Mesh *const m)
{
        /* Setup counting sort */
        /* The range of possible values is the num vertices */
        uint32_t *counters = (uint32_t *)calloc(m->num_vertices + 1, sizeof(uint32_t));

        for (uint32_t i = 0; i < m->num_faces; i++)
        {
                ++counters[m->faces[i].v1];
        }

        for (uint32_t i = 1; i <= m->num_vertices; i++)
        {
                counters[i] += counters[i - 1];
        }

        /* First iteration of sorting, based off v1 */
        uint32_t *sorted_face_handles = (uint32_t *)malloc(sizeof(uint32_t) * m->num_faces);

        for (uint32_t i = m->num_faces; i--;)
        {
                sorted_face_handles[--counters[m->faces[i].v1]] = i;
        }

        // for (uint32_t i = 0; i < m->num_vertices; i++)
        // {
        //         if (!counters[i])
        //         {
        //                 continue;
        //         }

        // }

        /* Populate adjacency data based off first sorting iteration */
        memset(counters, 0, (m->num_vertices + 1) * sizeof(uint32_t));
        uint32_t *current = sorted_face_handles, *end = sorted_face_handles, *duplicate_map = counters;
        while (end < sorted_face_handles + m->num_faces)
        {
                uint32_t current_v1 = m->faces[*current].v1;
                while (m->faces[*++end].v1 == current_v1)
                        ;

                while (current < end)
                {
                        uint32_t current_v2 = m->faces[*current].v2;
                        if (duplicate_map[current_v2])
                        {
                                m->adjacency_data[*current].v1v2 = duplicate_map[current_v2];
                                m->adjacency_data[duplicate_map[current_v2]].v1v2 = *current;
                                current++;
                                continue;
                        }
                        uint32_t current_v3 = m->faces[*current].v3;
                        if (duplicate_map[current_v3])
                        {
                                m->adjacency_data[*current].v1v3 = duplicate_map[current_v3];
                                m->adjacency_data[duplicate_map[current_v3]].v1v3 = *current;
                                current++;
                                continue;
                        }
                        duplicate_map[current_v2] = *current;
                        duplicate_map[current_v3] = *current;
                        current++;
                }
        }

        /* Prepare second sorting iteration */
        memset(counters, 0, (m->num_vertices + 1) * sizeof(uint32_t));

        for (uint32_t i = 0; i < m->num_faces; i++)
        {
                ++counters[m->faces[i].v1];
        }

        for (uint32_t i = 1; i <= m->num_vertices; i++)
        {
                counters[i] += counters[i - 1];
        }

        /* Second sort */
        for (uint32_t i = m->num_faces; i--;)
        {
                sorted_face_handles[--counters[m->faces[i].v2]] = i;
        }

        /* Populate adjacency data based off second sorting iteration */
        memset(counters, 0, (m->num_vertices + 1) * sizeof(uint32_t));
        current = sorted_face_handles;
        end = sorted_face_handles;
        while (end < sorted_face_handles + m->num_faces)
        {
                uint32_t current_v2 = m->faces[*current].v2;
                while (m->faces[*++end].v2 == current_v2)
                        ;

                while (current < end)
                {
                        uint32_t current_v3 = m->faces[*current].v3;
                        if (duplicate_map[current_v3])
                        {
                                m->adjacency_data[*current].v2v3 = duplicate_map[current_v3];
                                m->adjacency_data[duplicate_map[current_v3]].v2v3 = *current;
                                current++;
                                continue;
                        }
                        duplicate_map[current_v3] = *current;
                        current++;
                }
        }

        free(counters);
        free(sorted_face_handles);
}

bool mesh_init(Mesh *const m, const char *const p)
{
        int fd = open(p, O_RDONLY);
        if (fd == -1)
        {
                return false;
        }

        struct stat status;
        if (fstat(fd, &status) == -1)
        {
                return false;
        }

        char *buffer = (char *)malloc((sizeof(char) + 1) * status.st_size);
        buffer[status.st_size] = '\0';
        read(fd, buffer, status.st_size);

        uint32_t num_faces = 0, num_vertices = 0;
        for (uint32_t i = 0; i < status.st_size - 1; i++)
        {
                if (buffer[i] == 'v' && buffer[i + 1] != 'n')
                {
                        num_vertices++;
                }
                else if (buffer[i] == 'f' && buffer[i + 1] != 'n')
                {
                        num_faces++;
                }
        }

        printf("Num faces: %u, num vertices: %u\n", num_faces, num_vertices);

        m->data = malloc(num_faces * sizeof(Face) + num_vertices * sizeof(Vertex) + num_faces * sizeof(AdjacencyData));
        m->adjacency_data = m->data;
        m->faces = (Face *)(m->data + num_faces * sizeof(AdjacencyData));
        m->vertices = (Vertex *)(m->faces + num_faces);

        m->num_faces = num_faces;
        m->num_vertices = num_vertices;

        /*
                TODO
                this is a very simple parser that will break on almost anything other than the teapot
        */
        char c;
        uint32_t i = 0, vertices_cursor = 0, faces_cursor = 0;
        while ((c = buffer[i++]) != '\0')
        {
                if (c == 'v')
                {
                        i++;
                        m->vertices[vertices_cursor].x = atof(&buffer[i]);
                        while (buffer[++i] != ' ')
                                ;
                        m->vertices[vertices_cursor].y = atof(&buffer[i]);
                        while (buffer[++i] != ' ')
                                ;
                        m->vertices[vertices_cursor].z = atof(&buffer[i]);
                        vertices_cursor++;
                }
                else if (c == 'f')
                {
                        i++;
                        m->faces[faces_cursor].v1 = atoi(&buffer[i]);
                        while (buffer[++i] != ' ')
                                ;
                        m->faces[faces_cursor].v2 = atoi(&buffer[i]);
                        while (buffer[++i] != ' ')
                                ;
                        m->faces[faces_cursor].v3 = atoi(&buffer[i]);
                        faces_cursor++;
                }
        }

        // const char *const delims = "\n";
        // char *token = strtok(buffer, delims);
        // uint32_t faces_cursor = 0, vertices_cursor = 0;
        // do
        // {
        //         if (*token == 'v' && *(token + 1) != 'n')
        //         {
        //                 token++;
        //                 m->vertices[vertices_cursor].x = atof(token);
        //                 while (*++token != ' ')
        //                         ;
        //                 m->vertices[vertices_cursor].y = atof(token);
        //                 while (*++token != ' ')
        //                         ;
        //                 m->vertices[vertices_cursor].z = atof(token);
        //                 vertices_cursor++;
        //         }
        //         else if (*token == 'f' && *(token + 1) != 'n')
        //         {
        //                 token++;
        //                 m->faces[faces_cursor].v1 = atoi(token);
        //                 while (*++token != ' ')
        //                         ;
        //                 m->faces[faces_cursor].v2 = atoi(token);
        //                 while (*++token != ' ')
        //                         ;
        //                 m->faces[faces_cursor].v3 = atoi(token);
        //                 faces_cursor++;
        //         }
        // } while ((token = strtok(NULL, delims)) != NULL);

        return true;
}

int main()
{

        srand(time(NULL));

        /*
                1) get uv map thing
                2) graph clustering based of faces
                3) triangulate the polygons made up



                OR

                you could use a higher dimension clustering thing to have it generate the generalization and cluster at the same time

        */

        /*
                nice delorean model:
                Polygons 6,936,061
                Vertices 7,001,432
        */

        // const char *const path = "/Users/macfarrell/Documents/3d test models/lucy.obj";
        // const char *const path = "/Users/macfarrell/vs code projects/heph/cheburashka (2).obj";
        // const char *const path = "/Users/macfarrell/Documents/3d test models/Hosmer.OBJ";
        // const char *const path = "/Users/macfarrell/Documents/3d test models/eyeball.obj";
        // const char *const path = "/Users/macfarrell/Documents/3d test models/cottage_obj.obj";
        const char *const path = "/Users/macfarrell/Documents/3d test models/teapot.obj";

        Mesh m = {};
        if (!mesh_init(&m, path))
        {
                fprintf(stderr, "Failed to get mesh.\n");
                return -1;
        }

        mesh_init_adjacency_data(&m);

        for (uint32_t i = 0; i < m.num_faces; i++)
        {
                printf("========\n");
                printf("face num %u: %u, %u, %u \n", i, m.faces[i].v1, m.faces[i].v2, m.faces[i].v3);
                printf("AJ v1v2: %u\n", m.adjacency_data[i].v1v2);
                printf("AJ v1v3: %u\n", m.adjacency_data[i].v1v3);
                printf("AJ v2v3: %u\n", m.adjacency_data[i].v2v3);
        }

        free(m.data);
        return 0;
}
