cat > jogo.c << 'E0F'
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <sys/ioctl.h>

/* ╔════════════════════════════════════════════════════════════════╗ */
/* ║              JOGO EM C - TERMINAL COMPLETO                    ║ */
/* ║   Personagem + Monstros Aleatórios + Monstros Inteligentes    ║ */
/* ╚════════════════════════════════════════════════════════════════╝ */

// ========== CORES E CÓDIGOS ANSI ==========
#define ANSI_CURSOR_HOME "\033[H"
#define ANSI_COLOR_RESET "\033[0m"
#define ANSI_COLOR_RED "\033[31m"
#define ANSI_COLOR_GREEN "\033[32m"
#define ANSI_COLOR_BLUE "\033[34m"
#define ANSI_COLOR_YELLOW "\033[33m"
#define ANSI_COLOR_CYAN "\033[36m"
#define ANSI_COLOR_WHITE "\033[37m"
#define ANSI_BOLD "\033[1m"
#define HIDE_CURSOR "\033[?25l"
#define SHOW_CURSOR "\033[?25h"

// ========== STRUCTS ==========

// Mapa flexível
typedef struct {
    int linhas;
    int colunas;
    char dados[];
} Mapa;

// Estados do jogo
typedef enum {
    STATE_INIT,
    STATE_MENU,
    STATE_JOGANDO,
    STATE_PAUSA,
    STATE_GAME_OVER,
    STATE_VITORIA,
    STATE_EXIT
} GameState;

// Direções
typedef enum {
    DIR_UP = 0,
    DIR_DOWN,
    DIR_LEFT,
    DIR_RIGHT,
    DIR_NONE
} Direcao;

// Jogador
typedef struct {
    int x, y;
    Direcao direcao;
    char caractere;
} Jogador;

// Tipos de monstro
typedef enum {
    MONSTER_TYPE_X,  // Aleatório
    MONSTER_TYPE_Y   // Inteligente
} MonsterType;

// Monstro
typedef struct {
    MonsterType tipo;
    int x, y;
    char caractere;
    int vivo;
    int dano;
    Direcao ultima_direcao;
} Monstro;

// Grupo de monstros
typedef struct {
    Monstro *monstros;
    int quantidade;
    int max_monstros;
} GrupoMonstros;

// Contexto do jogo
typedef struct {
    GameState estado;
    Jogador jogador;
    GrupoMonstros *monstros;
    Mapa *mapa;
    int vidas;
    int score;
    int turno;
    int inimigos_derrotados;
} GameContext;

// ========== TERMINAL ==========

static struct termios original_settings;

void salvar_configuracao_terminal() {
    tcgetattr(STDIN_FILENO, &original_settings);
}

void habilitar_modo_raw() {
    struct termios new_settings = original_settings;
    new_settings.c_lflag &= ~(ICANON | ECHO);
    new_settings.c_cc[VMIN] = 0;
    new_settings.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &new_settings);
}

void desabilitar_modo_raw() {
    tcsetattr(STDIN_FILENO, TCSANOW, &original_settings);
}

int kbhit() {
    int bytes_disponiveis;
    ioctl(STDIN_FILENO, FIONREAD, &bytes_disponiveis);
    return bytes_disponiveis > 0;
}

int getch_nao_bloqueante() {
    unsigned char c;
    if (read(STDIN_FILENO, &c, 1) > 0) {
        return c;
    }
    return -1;
}

// ========== FUNÇÕES DE MAPA ==========

Mapa* criar_mapa(int linhas, int colunas) {
    Mapa *mapa = malloc(sizeof(Mapa) + linhas * colunas * sizeof(char));
    if (mapa) {
        mapa->linhas = linhas;
        mapa->colunas = colunas;
        memset(mapa->dados, ' ', linhas * colunas * sizeof(char));
    }
    return mapa;
}

#define ACESSAR(mapa, i, j) ((mapa)->dados[(i) * (mapa)->colunas + (j)])

void desenhar_bordas(Mapa *mapa) {
    for (int j = 0; j < mapa->colunas; j++) {
        ACESSAR(mapa, 0, j) = '#';
        ACESSAR(mapa, mapa->linhas - 1, j) = '#';
    }
    for (int i = 0; i < mapa->linhas; i++) {
        ACESSAR(mapa, i, 0) = '#';
        ACESSAR(mapa, i, mapa->colunas - 1) = '#';
    }
}

void desenhar_obstaculos(Mapa *mapa) {
    // Adicionar alguns obstáculos aleatórios
    for (int i = 5; i < mapa->linhas - 5; i += 5) {
        for (int j = 5; j < mapa->colunas - 5; j += 7) {
            ACESSAR(mapa, i, j) = '#';
            if (j + 1 < mapa->colunas - 1)
                ACESSAR(mapa, i, j + 1) = '#';
        }
    }
}

void limpar_mapa_jogavel(Mapa *mapa) {
    for (int i = 1; i < mapa->linhas - 1; i++) {
        for (int j = 1; j < mapa->colunas - 1; j++) {
            if (ACESSAR(mapa, i, j) == ' ' || ACESSAR(mapa, i, j) == '@' ||
                ACESSAR(mapa, i, j) == 'X' || ACESSAR(mapa, i, j) == 'Y' ||
                ACESSAR(mapa, i, j) == '^' || ACESSAR(mapa, i, j) == 'v' ||
                ACESSAR(mapa, i, j) == '<' || ACESSAR(mapa, i, j) == '>') {
                ACESSAR(mapa, i, j) = ' ';
            }
        }
    }
}

void desenhar_mapa_colorido(Mapa *mapa) {
    printf(ANSI_CURSOR_HOME);
    printf(ANSI_COLOR_CYAN ANSI_BOLD);
    for (int j = 0; j < mapa->colunas + 2; j++) printf("═");
    printf(ANSI_COLOR_RESET "\n");

    for (int i = 0; i < mapa->linhas; i++) {
        printf(ANSI_COLOR_CYAN ANSI_BOLD "║" ANSI_COLOR_RESET);
        
        for (int j = 0; j < mapa->colunas; j++) {
            char c = ACESSAR(mapa, i, j);
            
            if (c == '@') {
                printf(ANSI_COLOR_GREEN ANSI_BOLD "@" ANSI_COLOR_RESET);
            } else if (c == 'X') {
                printf(ANSI_COLOR_YELLOW ANSI_BOLD "X" ANSI_COLOR_RESET);
            } else if (c == 'Y') {
                printf(ANSI_COLOR_RED ANSI_BOLD "Y" ANSI_COLOR_RESET);
            } else if (c == '#') {
                printf(ANSI_COLOR_BLUE "█" ANSI_COLOR_RESET);
            } else {
                printf("%c", c);
            }
        }
        printf(ANSI_COLOR_CYAN ANSI_BOLD "║" ANSI_COLOR_RESET "\n");
    }

    printf(ANSI_COLOR_CYAN ANSI_BOLD);
    for (int j = 0; j < mapa->colunas + 2; j++) printf("═");
    printf(ANSI_COLOR_RESET "\n");
    fflush(stdout);
}

void liberar_mapa(Mapa *mapa) {
    free(mapa);
}

// ========== FUNÇÕES DE MONSTROS ==========

GrupoMonstros* criar_grupo_monstros(int max) {
    GrupoMonstros *grupo = malloc(sizeof(GrupoMonstros));
    grupo->monstros = malloc(max * sizeof(Monstro));
    grupo->quantidade = 0;
    grupo->max_monstros = max;
    return grupo;
}

void adicionar_monstro(GrupoMonstros *grupo, MonsterType tipo, int x, int y) {
    if (grupo->quantidade < grupo->max_monstros) {
        Monstro *m = &grupo->monstros[grupo->quantidade];
        m->tipo = tipo;
        m->x = x;
        m->y = y;
        m->vivo = 1;
        m->dano = (tipo == MONSTER_TYPE_X) ? 10 : 15;
        m->caractere = (tipo == MONSTER_TYPE_X) ? 'X' : 'Y';
        m->ultima_direcao = DIR_UP;
        grupo->quantidade++;
    }
}

int eh_movimento_valido(int novo_x, int novo_y, int max_x, int max_y,
                        char mapa_dados[], int mapa_cols) {
    if (novo_x <= 0 || novo_x >= max_x - 1 || novo_y <= 0 || novo_y >= max_y - 1) {
        return 0;
    }

    if (mapa_dados[novo_y * mapa_cols + novo_x] == '#') {
        return 0;
    }

    return 1;
}

void atualizar_monstro_tipo_x(Monstro *m, char mapa_dados[],
                              int max_x, int max_y, int mapa_cols) {
    int direcoes[4] = {DIR_UP, DIR_DOWN, DIR_LEFT, DIR_RIGHT};

    // Embaralhar
    for (int i = 3; i > 0; i--) {
        int j = rand() % (i + 1);
        int temp = direcoes[i];
        direcoes[i] = direcoes[j];
        direcoes[j] = temp;
    }

    for (int i = 0; i < 4; i++) {
        int novo_x = m->x;
        int novo_y = m->y;

        switch (direcoes[i]) {
            case DIR_UP:
                novo_y--;
                break;
            case DIR_DOWN:
                novo_y++;
                break;
            case DIR_LEFT:
                novo_x--;
                break;
            case DIR_RIGHT:
                novo_x++;
                break;
        }

        if (eh_movimento_valido(novo_x, novo_y, max_x, max_y, mapa_dados, mapa_cols)) {
            m->x = novo_x;
            m->y = novo_y;
            m->ultima_direcao = direcoes[i];
            return;
        }
    }
}

void atualizar_monstro_tipo_y(Monstro *m, int jogador_x, int jogador_y,
                              char mapa_dados[],
                              int max_x, int max_y, int mapa_cols) {
    int dist_x = jogador_x - m->x;
    int dist_y = jogador_y - m->y;
    int manhattan = abs(dist_x) + abs(dist_y);

    int RAIO_DETECCAO = 12;
    if (manhattan > RAIO_DETECCAO) {
        atualizar_monstro_tipo_x(m, mapa_dados, max_x, max_y, mapa_cols);
        return;
    }

    // Prioridade: reduzir distância maior
    if (abs(dist_x) > abs(dist_y)) {
        if (dist_x > 0) {
            int novo_x = m->x + 1;
            if (eh_movimento_valido(novo_x, m->y, max_x, max_y, mapa_dados, mapa_cols)) {
                m->x = novo_x;
                m->ultima_direcao = DIR_RIGHT;
                return;
            }
        } else if (dist_x < 0) {
            int novo_x = m->x - 1;
            if (eh_movimento_valido(novo_x, m->y, max_x, max_y, mapa_dados, mapa_cols)) {
                m->x = novo_x;
                m->ultima_direcao = DIR_LEFT;
                return;
            }
        }
    } else if (abs(dist_y) > 0) {
        if (dist_y > 0) {
            int novo_y = m->y + 1;
            if (eh_movimento_valido(m->x, novo_y, max_x, max_y, mapa_dados, mapa_cols)) {
                m->y = novo_y;
                m->ultima_direcao = DIR_DOWN;
                return;
            }
        } else if (dist_y < 0) {
            int novo_y = m->y - 1;
            if (eh_movimento_valido(m->x, novo_y, max_x, max_y, mapa_dados, mapa_cols)) {
                m->y = novo_y;
                m->ultima_direcao = DIR_UP;
                return;
            }
        }
    }

    // Tentar direção alternativa
    if (abs(dist_x) > abs(dist_y)) {
        if (dist_y > 0) {
            int novo_y = m->y + 1;
            if (eh_movimento_valido(m->x, novo_y, max_x, max_y, mapa_dados, mapa_cols)) {
                m->y = novo_y;
                m->ultima_direcao = DIR_DOWN;
                return;
            }
        } else if (dist_y < 0) {
            int novo_y = m->y - 1;
            if (eh_movimento_valido(m->x, novo_y, max_x, max_y, mapa_dados, mapa_cols)) {
                m->y = novo_y;
                m->ultima_direcao = DIR_UP;
                return;
            }
        }
    }
}

void atualizar_monstros(GrupoMonstros *grupo, int jogador_x, int jogador_y,
                       char mapa_dados[], int max_x, int max_y, int mapa_cols) {
    for (int i = 0; i < grupo->quantidade; i++) {
        Monstro *m = &grupo->monstros[i];
        if (!m->vivo) continue;

        if (m->tipo == MONSTER_TYPE_X) {
            atualizar_monstro_tipo_x(m, mapa_dados, max_x, max_y, mapa_cols);
        } else if (m->tipo == MONSTER_TYPE_Y) {
            atualizar_monstro_tipo_y(m, jogador_x, jogador_y,
                                     mapa_dados, max_x, max_y, mapa_cols);
        }
    }
}

int verificar_colisao(GrupoMonstros *grupo, int jogador_x, int jogador_y,
                      int *dano_total) {
    int colidiu = 0;
    *dano_total = 0;

    for (int i = 0; i < grupo->quantidade; i++) {
        Monstro *m = &grupo->monstros[i];
        if (m->vivo && m->x == jogador_x && m->y == jogador_y) {
            colidiu = 1;
            *dano_total += m->dano;
            m->vivo = 0;
        }
    }

    return colidiu;
}

void liberar_monstros(GrupoMonstros *grupo) {
    if (grupo) {
        free(grupo->monstros);
        free(grupo);
    }
}

// ========== GAME LOOP ==========

void mostrar_menu() {
    system("clear");
    printf("\n");
    printf("  ╔═══════════════════════════════════════════════════════╗\n");
    printf("  ║                    \033[32m🎮 JOGO EM C 🎮\033[0m                      ║\n");
    printf("  ║                Monstros & Sobrevivência              ║\n");
    printf("  ╚═══════════════════════════════════════════════════════╝\n");
    printf("\n");
    printf("  ┌─────────────────────────────────────────────────────┐\n");
    printf("  │                   INSTRUÇÕES                        │\n");
    printf("  ├─────────────────────────────────────────────────────┤\n");
    printf("  │  🎮 CONTROLES:                                      │\n");
    printf("  │     W   → Mover para cima (^)                       │\n");
    printf("  │     S   → Mover para baixo (v)                      │\n");
    printf("  │     A   → Mover para esquerda (<)                   │\n");
    printf("  │     D   → Mover para direita (>)                    │\n");
    printf("  │     P   → Pausar/Retomar                            │\n");
    printf("  │     Q   → Sair                                      │\n");
    printf("  │                                                     │\n");
    printf("  │  👾 MONSTROS:                                       │\n");
    printf("  │     \033[33mX\033[0m = Tipo X (Movimento ALEATÓRIO) - 10 dano   │\n");
    printf("  │     \033[31mY\033[0m = Tipo Y (SEGUE O JOGADOR)   - 15 dano   │\n");
    printf("  │                                                     │\n");
    printf("  │  ❤️  VOCÊ COMEÇA COM 3 VIDAS                        │\n");
    printf("  │  📊 GANHE PONTOS DERROTANDO MONSTROS                │\n");
    printf("  │                                                     │\n");
    printf("  └─────────────────────────────────────────────────────┘\n");
    printf("\n  Pressione ENTER para começar...");
    fflush(stdout);
    getchar();
}

void mostrar_game_over(GameContext *ctx) {
    system("clear");
    printf("\n");
    printf("  ╔═══════════════════════════════════════════════════════╗\n");
    printf("  ║                    \033[31m💀 GAME OVER 💀\033[0m                    ║\n");
    printf("  ╚═══════════════════════════════════════════════════════╝\n");
    printf("\n");
    printf("  ┌─────────────────────────────────────────────────────┐\n");
    printf("  │  📊 ESTATÍSTICAS FINAIS                             │\n");
    printf("  ├─────────────────────────────────────────────────────┤\n");
    printf("  │  Turnos sobrevividos: %-28d │\n", ctx->turno);
    printf("  │  Monstros derrotados: %-28d │\n", ctx->inimigos_derrotados);
    printf("  │  Score final:         %-28d │\n", ctx->score);
    printf("  │  Vidas restantes:     %-28d │\n", ctx->vidas);
    printf("  └─────────────────────────────────────────────────────┘\n");
    printf("\n  Pressione ENTER para voltar ao menu...");
    fflush(stdout);
    getchar();
}

void mostrar_vitoria(GameContext *ctx) {
    system("clear");
    printf("\n");
    printf("  ╔═══════════════════════════════════════════════════════╗\n");
    printf("  ║                   \033[32m✨ VITÓRIA! ✨\033[0m                   ║\n");
    printf("  ║              Você derrotou todos os monstros!        ║\n");
    printf("  ╚═══════════════════════════════════════════════════════╝\n");
    printf("\n");
    printf("  ┌─────────────────────────────────────────────────────┐\n");
    printf("  │  📊 ESTATÍSTICAS FINAIS                             │\n");
    printf("  ├─────────────────────────────────────────────────────┤\n");
    printf("  │  Turnos para vitória:  %-28d │\n", ctx->turno);
    printf("  │  Monstros derrotados:  %-28d │\n", ctx->inimigos_derrotados);
    printf("  │  Score final:          %-28d │\n", ctx->score);
    printf("  │  Vidas restantes:      %-28d │\n", ctx->vidas);
    printf("  └─────────────────────────────────────────────────────┘\n");
    printf("\n  Pressione ENTER para voltar ao menu...");
    fflush(stdout);
    getchar();
}

void processar_input(GameContext *ctx) {
    if (!kbhit()) return;

    int c = getch_nao_bloqueante();
    if (c == -1) return;

    if (c >= 'A' && c <= 'Z') c += 32;  // Converter para minúscula

    switch (c) {
        case 'w':
            ctx->jogador.direcao = DIR_UP;
            ctx->jogador.caractere = '^';
            if (eh_movimento_valido(ctx->jogador.x, ctx->jogador.y - 1,
                                   ctx->mapa->colunas, ctx->mapa->linhas,
                                   ctx->mapa->dados, ctx->mapa->colunas)) {
                ctx->jogador.y--;
            }
            break;
        case 's':
            ctx->jogador.direcao = DIR_DOWN;
            ctx->jogador.caractere = 'v';
            if (eh_movimento_valido(ctx->jogador.x, ctx->jogador.y + 1,
                                   ctx->mapa->colunas, ctx->mapa->linhas,
                                   ctx->mapa->dados, ctx->mapa->colunas)) {
                ctx->jogador.y++;
            }
            break;
        case 'a':
            ctx->jogador.direcao = DIR_LEFT;
            ctx->jogador.caractere = '<';
            if (eh_movimento_valido(ctx->jogador.x - 1, ctx->jogador.y,
                                   ctx->mapa->colunas, ctx->mapa->linhas,
                                   ctx->mapa->dados, ctx->mapa->colunas)) {
                ctx->jogador.x--;
            }
            break;
        case 'd':
            ctx->jogador.direcao = DIR_RIGHT;
            ctx->jogador.caractere = '>';
            if (eh_movimento_valido(ctx->jogador.x + 1, ctx->jogador.y,
                                   ctx->mapa->colunas, ctx->mapa->linhas,
                                   ctx->mapa->dados, ctx->mapa->colunas)) {
                ctx->jogador.x++;
            }
            break;
        case 'p':
            ctx->estado = (ctx->estado == STATE_JOGANDO) ? STATE_PAUSA : STATE_JOGANDO;
            break;
        case 'q':
            ctx->estado = STATE_EXIT;
            break;
    }
}

void atualizar_jogo(GameContext *ctx) {
    // Limpar mapa
    limpar_mapa_jogavel(ctx->mapa);

    // Atualizar monstros
    atualizar_monstros(ctx->monstros, ctx->jogador.x, ctx->jogador.y,
                      ctx->mapa->dados, ctx->mapa->colunas, ctx->mapa->linhas,
                      ctx->mapa->colunas);

    // Desenhar monstros
    for (int i = 0; i < ctx->monstros->quantidade; i++) {
        Monstro *m = &ctx->monstros->monstros[i];
        if (m->vivo && m->x > 0 && m->x < ctx->mapa->colunas - 1 &&
            m->y > 0 && m->y < ctx->mapa->linhas - 1) {
            ACESSAR(ctx->mapa, m->y, m->x) = m->caractere;
        }
    }

    // Desenhar jogador
    ACESSAR(ctx->mapa, ctx->jogador.y, ctx->jogador.x) = ctx->jogador.caractere;

    // Verificar colisão
    int dano_total = 0;
    if (verificar_colisao(ctx->monstros, ctx->jogador.x, ctx->jogador.y, &dano_total)) {
        ctx->vidas--;
        ctx->score -= dano_total;
        if (ctx->score < 0) ctx->score = 0;
    }

    // Contar monstros vivos
    int vivos = 0;
    for (int i = 0; i < ctx->monstros->quantidade; i++) {
        if (ctx->monstros->monstros[i].vivo) {
            vivos++;
        } else {
            ctx->inimigos_derrotados++;
            ctx->score += 50;
        }
    }

    // Verificar condições de fim
    if (ctx->vidas <= 0) {
        ctx->estado = STATE_GAME_OVER;
    } else if (vivos == 0 && ctx->turno > 10) {
        ctx->estado = STATE_VITORIA;
    }

    ctx->turno++;
}

void renderizar_jogo(GameContext *ctx) {
    desenhar_mapa_colorido(ctx->mapa);

    printf("\n");
    printf("  ┌──────────────────────────────────────────────────────────────────────┐\n");
    printf("  │ ❤️  VIDAS: %d  │  📊 SCORE: %d  │  ⚔️  MONSTROS VIVOS: %d/5  │ TURNO: %d  │\n",
           ctx->vidas, ctx->score,
           (ctx->monstros->quantidade - ctx->inimigos_derrotados),
           ctx->turno);
    printf("  │ 📍 POS: (%d, %d)  │  🎮 Controles: W/A/S/D | P-Pausar | Q-Sair       │\n",
           ctx->jogador.x, ctx->jogador.y);
    printf("  └──────────────────────────────────────────────────────────────────────┘\n");

    fflush(stdout);
}

int main() {
    srand(time(NULL));
    salvar_configuracao_terminal();
    habilitar_modo_raw();
    printf(HIDE_CURSOR);

    GameContext ctx = {0};
    ctx.estado = STATE_INIT;
    ctx.vidas = 3;
    ctx.score = 0;
    ctx.turno = 0;
    ctx.inimigos_derrotados = 0;

    while (ctx.estado != STATE_EXIT) {
        switch (ctx.estado) {
            case STATE_INIT:
                mostrar_menu();
                ctx.mapa = criar_mapa(20, 60);
                desenhar_bordas(ctx.mapa);
                desenhar_obstaculos(ctx.mapa);

                ctx.jogador.x = 30;
                ctx.jogador.y = 10;
                ctx.jogador.caractere = '@';
                ctx.jogador.direcao = DIR_UP;

                ctx.monstros = criar_grupo_monstros(5);
                adicionar_monstro(ctx.monstros, MONSTER_TYPE_X, 10, 5);
                adicionar_monstro(ctx.monstros, MONSTER_TYPE_X, 50, 15);
                adicionar_monstro(ctx.monstros, MONSTER_TYPE_Y, 15, 8);
                adicionar_monstro(ctx.monstros, MONSTER_TYPE_Y, 45, 12);
                adicionar_monstro(ctx.monstros, MONSTER_TYPE_Y, 25, 18);

                ctx.estado = STATE_JOGANDO;
                ctx.vidas = 3;
                ctx.score = 0;
                ctx.turno = 0;
                ctx.inimigos_derrotados = 0;
                break;

            case STATE_JOGANDO:
                processar_input(&ctx);
                atualizar_jogo(&ctx);
                renderizar_jogo(&ctx);
                usleep(200000);  // 200ms
                break;

            case STATE_PAUSA:
                printf(ANSI_CURSOR_HOME);
                printf("\n\n\n  \033[33m⏸️  JOGO PAUSADO\033[0m\n\n  Pressione P para retomar ou Q para sair...\n");
                fflush(stdout);
                processar_input(&ctx);
                usleep(100000);
                break;

            case STATE_GAME_OVER:
                liberar_mapa(ctx.mapa);
                liberar_monstros(ctx.monstros);
                mostrar_game_over(&ctx);
                ctx.estado = STATE_INIT;
                break;

            case STATE_VITORIA:
                liberar_mapa(ctx.mapa);
                liberar_monstros(ctx.monstros);
                mostrar_vitoria(&ctx);
                ctx.estado = STATE_INIT;
                break;

            default:
                ctx.estado = STATE_EXIT;
        }
    }

    printf(SHOW_CURSOR);
    desabilitar_modo_raw();
    system("clear");
    printf("\nObrigado por jogar! 🎮\n\n");

    return 0;
}
