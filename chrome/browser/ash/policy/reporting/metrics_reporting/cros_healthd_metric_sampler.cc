// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_metric_sampler.h"

#include "base/logging.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_sampler_handlers/cros_healthd_audio_sampler_handler.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_sampler_handlers/cros_healthd_boot_performance_sampler_handler.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_sampler_handlers/cros_healthd_bus_sampler_handler.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_sampler_handlers/cros_healthd_cpu_sampler_handler.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_sampler_handlers/cros_healthd_input_sampler_handler.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/service_connection.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace reporting {

namespace {

namespace cros_healthd = ::ash::cros_healthd::mojom;

MemoryEncryptionState TranslateMemoryEncryptionState(
    cros_healthd::EncryptionState encryption_state) {
  switch (encryption_state) {
    case cros_healthd::EncryptionState::kUnknown:
      return MEMORY_ENCRYPTION_STATE_UNKNOWN;
    case cros_healthd::EncryptionState::kEncryptionDisabled:
      return MEMORY_ENCRYPTION_STATE_DISABLED;
    case cros_healthd::EncryptionState::kTmeEnabled:
      return MEMORY_ENCRYPTION_STATE_TME;
    case cros_healthd::EncryptionState::kMktmeEnabled:
      return MEMORY_ENCRYPTION_STATE_MKTME;
  }

  NOTREACHED();
}

MemoryEncryptionAlgorithm TranslateMemoryEncryptionAlgorithm(
    cros_healthd::CryptoAlgorithm encryption_algorithm) {
  switch (encryption_algorithm) {
    case cros_healthd::CryptoAlgorithm::kUnknown:
      return MEMORY_ENCRYPTION_ALGORITHM_UNKNOWN;
    case cros_healthd::CryptoAlgorithm::kAesXts128:
      return MEMORY_ENCRYPTION_ALGORITHM_AES_XTS_128;
    case cros_healthd::CryptoAlgorithm::kAesXts256:
      return MEMORY_ENCRYPTION_ALGORITHM_AES_XTS_256;
  }

  NOTREACHED();
}

void HandleMemoryResult(OptionalMetricCallback callback,
                        CrosHealthdMetricSampler::MetricType metric_type,
                        cros_healthd::TelemetryInfoPtr result) {
  absl::optional<MetricData> metric_data;
  const auto& memory_result = result->memory_result;

  if (!memory_result.is_null()) {
    switch (memory_result->which()) {
      case cros_healthd::MemoryResult::Tag::kError: {
        DVLOG(1) << "cros_healthd: Error getting memory info: "
                 << memory_result->get_error()->msg;
        break;
      }

      case cros_healthd::MemoryResult::Tag::kMemoryInfo: {
        const auto& memory_info = memory_result->get_memory_info();
        if (memory_result.is_null()) {
          DVLOG(1) << "Null MemoryInfo from cros_healthd";
          break;
        }

        // Gather memory info.
        if (metric_type == CrosHealthdMetricSampler::MetricType::kInfo) {
          metric_data = absl::make_optional<MetricData>();
          auto* const memory_encryption_info_out =
              metric_data->mutable_info_data()
                  ->mutable_memory_info()
                  ->mutable_tme_info();
          const auto* const memory_encryption_info =
              memory_info->memory_encryption_info.get();

          if (memory_encryption_info) {
            memory_encryption_info_out->set_encryption_state(
                TranslateMemoryEncryptionState(
                    memory_encryption_info->encryption_state));
            memory_encryption_info_out->set_encryption_algorithm(
                TranslateMemoryEncryptionAlgorithm(
                    memory_encryption_info->active_algorithm));
            memory_encryption_info_out->set_max_keys(
                memory_encryption_info->max_key_number);
            memory_encryption_info_out->set_key_length(
                memory_encryption_info->key_length);
          } else {
            // If encryption info isn't set, mark it as disabled.
            memory_encryption_info_out->set_encryption_state(
                MEMORY_ENCRYPTION_STATE_DISABLED);
          }
        }
        break;
      }
    }
  }

  std::move(callback).Run(std::move(metric_data));
}

void HandleDisplayResult(OptionalMetricCallback callback,
                         CrosHealthdMetricSampler::MetricType metric_type,
                         cros_healthd::TelemetryInfoPtr result) {
  absl::optional<MetricData> metric_data;
  const auto& display_result = result->display_result;
  if (!display_result.is_null()) {
    switch (display_result->which()) {
      case cros_healthd::DisplayResult::Tag::kError: {
        DVLOG(1) << "cros_healthd: Error getting bus info: "
                 << display_result->get_error()->msg;
        break;
      }

      case cros_healthd::DisplayResult::Tag::kDisplayInfo: {
        const auto& display_info = display_result->get_display_info();
        if (display_info.is_null()) {
          DVLOG(1) << "Null DisplayInfo from cros_healthd";
          break;
        }

        metric_data = absl::make_optional<MetricData>();
        const auto* const embedded_display_info = display_info->edp_info.get();
        if (metric_type == CrosHealthdMetricSampler::MetricType::kInfo) {
          // Gather e-privacy screen info.
          auto* const privacy_screen_info_out =
              metric_data->mutable_info_data()->mutable_privacy_screen_info();
          privacy_screen_info_out->set_supported(
              embedded_display_info->privacy_screen_supported);

          // Gather displays info.
          auto* const internal_dp_out = metric_data->mutable_info_data()
                                            ->mutable_display_info()
                                            ->add_display_device();
          internal_dp_out->set_is_internal(true);
          if (embedded_display_info->display_name.has_value()) {
            internal_dp_out->set_display_name(
                embedded_display_info->display_name.value());
          }
          if (embedded_display_info->display_width) {
            internal_dp_out->set_display_width(
                embedded_display_info->display_width->value);
          }
          if (embedded_display_info->display_height) {
            internal_dp_out->set_display_height(
                embedded_display_info->display_height->value);
          }
          if (embedded_display_info->manufacturer.has_value()) {
            internal_dp_out->set_manufacturer(
                embedded_display_info->manufacturer.value());
          }
          if (embedded_display_info->model_id) {
            internal_dp_out->set_model_id(
                embedded_display_info->model_id->value);
          }
          if (embedded_display_info->manufacture_year) {
            internal_dp_out->set_manufacture_year(
                embedded_display_info->manufacture_year->value);
          }
          if (display_info->dp_infos) {
            for (const auto& current_external_display :
                 *display_info->dp_infos) {
              auto* const external_dp_out = metric_data->mutable_info_data()
                                                ->mutable_display_info()
                                                ->add_display_device();
              external_dp_out->set_is_internal(false);
              if (current_external_display->display_name.has_value()) {
                external_dp_out->set_display_name(
                    current_external_display->display_name.value());
              }
              if (current_external_display->display_width) {
                external_dp_out->set_display_width(
                    current_external_display->display_width->value);
              }
              if (current_external_display->display_height) {
                external_dp_out->set_display_height(
                    current_external_display->display_height->value);
              }
              if (current_external_display->manufacturer.has_value()) {
                external_dp_out->set_manufacturer(
                    current_external_display->manufacturer.value());
              }
              if (current_external_display->model_id) {
                external_dp_out->set_model_id(
                    current_external_display->model_id->value);
              }
              if (current_external_display->manufacture_year) {
                external_dp_out->set_manufacture_year(
                    current_external_display->manufacture_year->value);
              }
            }
          }
        } else if (metric_type ==
                   CrosHealthdMetricSampler::MetricType::kTelemetry) {
          // Gather displays telemetry.
          auto* const internal_dp_out = metric_data->mutable_telemetry_data()
                                            ->mutable_displays_telemetry()
                                            ->add_display_status();
          internal_dp_out->set_is_internal(true);
          if (embedded_display_info->display_name.has_value()) {
            internal_dp_out->set_display_name(
                embedded_display_info->display_name.value());
          }
          if (embedded_display_info->resolution_horizontal) {
            internal_dp_out->set_resolution_horizontal(
                embedded_display_info->resolution_horizontal->value);
          }
          if (embedded_display_info->resolution_vertical) {
            internal_dp_out->set_resolution_vertical(
                embedded_display_info->resolution_vertical->value);
          }
          if (embedded_display_info->refresh_rate) {
            internal_dp_out->set_refresh_rate(
                embedded_display_info->refresh_rate->value);
          }
          if (display_info->dp_infos) {
            for (const auto& current_external_display :
                 *display_info->dp_infos) {
              auto* const external_dp_out =
                  metric_data->mutable_telemetry_data()
                      ->mutable_displays_telemetry()
                      ->add_display_status();
              external_dp_out->set_is_internal(false);
              if (current_external_display->display_name.has_value()) {
                external_dp_out->set_display_name(
                    current_external_display->display_name.value());
              }
              if (current_external_display->resolution_horizontal) {
                external_dp_out->set_resolution_horizontal(
                    current_external_display->resolution_horizontal->value);
              }
              if (current_external_display->resolution_vertical) {
                external_dp_out->set_resolution_vertical(
                    current_external_display->resolution_vertical->value);
              }
              if (current_external_display->refresh_rate) {
                external_dp_out->set_refresh_rate(
                    current_external_display->refresh_rate->value);
              }
            }
          }
        }
        break;
      }
    }
  }
  std::move(callback).Run(std::move(metric_data));
}

void OnHealthdInfoReceived(OptionalMetricCallback callback,
                           cros_healthd::ProbeCategoryEnum probe_category,
                           CrosHealthdMetricSampler::MetricType metric_type,
                           cros_healthd::TelemetryInfoPtr result) {
  DCHECK(result);
  switch (probe_category) {
    case cros_healthd::ProbeCategoryEnum::kAudio: {
      CrosHealthdAudioSamplerHandler handler = CrosHealthdAudioSamplerHandler();
      handler.HandleResult(std::move(result), std::move(callback));
      break;
    }
    case cros_healthd::ProbeCategoryEnum::kBus: {
      CrosHealthdBusSamplerHandler handler = CrosHealthdBusSamplerHandler(metric_type);
      handler.HandleResult(std::move(result), std::move(callback));
      break;
    }
    case cros_healthd::ProbeCategoryEnum::kCpu: {
      CrosHealthdCpuSamplerHandler handler = CrosHealthdCpuSamplerHandler();
      handler.HandleResult(std::move(result), std::move(callback));
      break;
    }
    case cros_healthd::ProbeCategoryEnum::kMemory: {
      HandleMemoryResult(std::move(callback), metric_type, std::move(result));
      break;
    }
    case cros_healthd::ProbeCategoryEnum::kBootPerformance: {
      CrosHealthdBootPerformanceSamplerHandler handler =
          CrosHealthdBootPerformanceSamplerHandler();
      handler.HandleResult(std::move(result), std::move(callback));
      break;
    }
    case cros_healthd::ProbeCategoryEnum::kInput: {
      CrosHealthdInputSamplerHandler handler = CrosHealthdInputSamplerHandler();
      handler.HandleResult(std::move(result), std::move(callback));
      break;
    }
    case cros_healthd::ProbeCategoryEnum::kDisplay: {
      HandleDisplayResult(std::move(callback), metric_type, std::move(result));
      break;
    }
    default: {
      NOTREACHED();
      return;
    }
  }
}

}  // namespace

CrosHealthdMetricSampler::CrosHealthdMetricSampler(
    cros_healthd::ProbeCategoryEnum probe_category,
    CrosHealthdMetricSampler::MetricType metric_type)
    : probe_category_(probe_category), metric_type_(metric_type) {}

CrosHealthdMetricSampler::~CrosHealthdMetricSampler() = default;

void CrosHealthdMetricSampler::MaybeCollect(OptionalMetricCallback callback) {
  auto healthd_callback =
      base::BindOnce(OnHealthdInfoReceived, std::move(callback),
                     probe_category_, metric_type_);
  ash::cros_healthd::ServiceConnection::GetInstance()
      ->GetProbeService()
      ->ProbeTelemetryInfo(
          std::vector<cros_healthd::ProbeCategoryEnum>{probe_category_},
          std::move(healthd_callback));
}

}  // namespace reporting
