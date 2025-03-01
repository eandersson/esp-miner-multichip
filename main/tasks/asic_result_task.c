#include "system.h"
#include "work_queue.h"
#include "serial.h"
#include <string.h>
#include "esp_log.h"
#include "nvs_config.h"
#include "utils.h"

static const char *TAG = "asic_result";

void ASIC_result_task(void *pvParameters)
{
    GlobalState *GLOBAL_STATE = (GlobalState *)pvParameters;

    char *user = nvs_config_get_string(NVS_CONFIG_STRATUM_USER, STRATUM_USER);

    while (1)
    {

        task_result *asic_result = (*GLOBAL_STATE->ASIC_functions.receive_result_fn)(GLOBAL_STATE);

        if (asic_result == NULL)
        {
            continue;
        }

        uint8_t job_id = asic_result->job_id;

        if (GLOBAL_STATE->valid_jobs[job_id] == 0)
        {
            ESP_LOGI(TAG, "Invalid job nonce found, 0x%02X", job_id);
            continue;
        }

        // check the nonce difficulty
        double nonce_diff = test_nonce_value(
            GLOBAL_STATE->ASIC_TASK_MODULE.active_jobs[job_id],
            asic_result->nonce,
            asic_result->rolled_version);

        uint32_t pool_difficulty = GLOBAL_STATE->ASIC_TASK_MODULE.active_jobs[job_id]->pool_diff;

        //log the ASIC response
        ESP_LOGI(TAG, "AsicNr: %d Ver: %08" PRIX32 " Nonce %08" PRIX32 " diff %.1f of %ld.", asic_result->asic_nr,asic_result->rolled_version, asic_result->nonce, nonce_diff, pool_difficulty);

        if (nonce_diff > pool_difficulty)
        {

            STRATUM_V1_submit_share(
                GLOBAL_STATE->sock,
                user,
                GLOBAL_STATE->ASIC_TASK_MODULE.active_jobs[job_id]->jobid,
                GLOBAL_STATE->ASIC_TASK_MODULE.active_jobs[job_id]->extranonce2,
                GLOBAL_STATE->ASIC_TASK_MODULE.active_jobs[job_id]->ntime,
                asic_result->nonce,
                asic_result->rolled_version ^ GLOBAL_STATE->ASIC_TASK_MODULE.active_jobs[job_id]->version);
        
        SYSTEM_notify_found_nonce(GLOBAL_STATE, (double) pool_difficulty);
        }

        SYSTEM_check_for_best_diff(GLOBAL_STATE, nonce_diff, job_id);
        
    }
}