// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_metric_sampler.h"

#include "base/logging.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/service_connection.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace reporting {

namespace {

namespace cros_healthd = ::ash::cros_healthd::mojom;

ThunderboltSecurityLevel TranslateThunderboltSecurityLevel(
    cros_healthd::ThunderboltSecurityLevel security_level) {
  switch (security_level) {
    case cros_healthd::ThunderboltSecurityLevel::kNone:
      return THUNDERBOLT_SECURITY_NONE_LEVEL;
    case cros_healthd::ThunderboltSecurityLevel::kUserLevel:
      return THUNDERBOLT_SECURITY_USER_LEVEL;
    case cros_healthd::ThunderboltSecurityLevel::kSecureLevel:
      return THUNDERBOLT_SECURITY_SECURE_LEVEL;
    case cros_healthd::ThunderboltSecurityLevel::kDpOnlyLevel:
      return THUNDERBOLT_SECURITY_DP_ONLY_LEVEL;
    case cros_healthd::ThunderboltSecurityLevel::kUsbOnlyLevel:
      return THUNDERBOLT_SECURITY_USB_ONLY_LEVEL;
    case cros_healthd::ThunderboltSecurityLevel::kNoPcieLevel:
      return THUNDERBOLT_SECURITY_NO_PCIE_LEVEL;
  }

  NOTREACHED();
}

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

void HandleBusResult(OptionalMetricCallback callback,
                     CrosHealthdMetricSampler::MetricType metric_type,
                     cros_healthd::TelemetryInfoPtr result) {
  absl::optional<MetricData> metric_data;
  const auto& bus_result = result->bus_result;

  if (!bus_result.is_null()) {
    switch (bus_result->which()) {
      case cros_healthd::BusResult::Tag::kError: {
        DVLOG(1) << "cros_healthd: Error getting bus info: "
                 << bus_result->get_error()->msg;
        break;
      }

      case cros_healthd::BusResult::Tag::kBusDevices: {
        for (const auto& bus_device : bus_result->get_bus_devices()) {
          const auto& bus_info = bus_device->bus_info;
          if (metric_type == CrosHealthdMetricSampler::MetricType::kInfo) {
            if (bus_info->is_thunderbolt_bus_info()) {
              if (!metric_data.has_value()) {
                metric_data = absl::make_optional<MetricData>();
              }
              auto* const thunderbolt_info_out =
                  metric_data->mutable_info_data()
                      ->mutable_bus_device_info()
                      ->add_thunderbolt_info();
              thunderbolt_info_out->set_security_level(
                  TranslateThunderboltSecurityLevel(
                      bus_info->get_thunderbolt_bus_info()->security_level));
            }
          } else if (metric_type ==
                     CrosHealthdMetricSampler::MetricType::kTelemetry) {
            if (bus_info->is_usb_bus_info()) {
              if (!metric_data.has_value()) {
                metric_data = absl::make_optional<MetricData>();
              }
              auto* const usb_telemetry_out =
                  metric_data->mutable_telemetry_data()
                      ->mutable_peripherals_telemetry()
                      ->add_usb_telemetry();
              usb_telemetry_out->set_vid(
                  bus_info->get_usb_bus_info()->vendor_id);
              usb_telemetry_out->set_pid(
                  bus_info->get_usb_bus_info()->product_id);
              usb_telemetry_out->set_class_id(
                  bus_info->get_usb_bus_info()->class_id);
              usb_telemetry_out->set_subclass_id(
                  bus_info->get_usb_bus_info()->subclass_id);
              usb_telemetry_out->set_vendor(bus_device->vendor_name);
              usb_telemetry_out->set_name(bus_device->product_name);
              if (bus_info->get_usb_bus_info()->fwupd_firmware_version_info) {
                usb_telemetry_out->set_firmware_version(
                    bus_info->get_usb_bus_info()
                        ->fwupd_firmware_version_info->version);
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

void HandleCpuResult(OptionalMetricCallback callback,
                     CrosHealthdMetricSampler::MetricType metric_type,
                     cros_healthd::TelemetryInfoPtr result) {
  absl::optional<MetricData> metric_data;
  const auto& cpu_result = result->cpu_result;

  if (!cpu_result.is_null()) {
    switch (cpu_result->which()) {
      case cros_healthd::CpuResult::Tag::kError: {
        DVLOG(1) << "cros_healthd: Error getting CPU info: "
                 << cpu_result->get_error()->msg;
        break;
      }

      case cros_healthd::CpuResult::Tag::kCpuInfo: {
        const auto& cpu_info = cpu_result->get_cpu_info();
        if (cpu_info.is_null()) {
          DVLOG(1) << "Null CpuInfo from cros_healthd";
          break;
        }

        // Gather keylocker info.
        if (metric_type == CrosHealthdMetricSampler::MetricType::kInfo) {
          metric_data = absl::make_optional<MetricData>();
          auto* const keylocker_info_out = metric_data->mutable_info_data()
                                               ->mutable_cpu_info()
                                               ->mutable_keylocker_info();
          const auto* const keylocker_info = cpu_info->keylocker_info.get();
          if (keylocker_info) {
            keylocker_info_out->set_supported(true);
            keylocker_info_out->set_configured(
                keylocker_info->keylocker_configured);
          } else {
            // If keylocker info isn't set, it is not supported on the board.
            keylocker_info_out->set_supported(false);
            keylocker_info_out->set_configured(false);
          }
        }
        break;
      }
    }
  }

  std::move(callback).Run(std::move(metric_data));
}

void HandleBootPerformanceResult(
    OptionalMetricCallback callback,
    CrosHealthdMetricSampler::MetricType metric_type,
    cros_healthd::TelemetryInfoPtr result) {
  const std::string kShutdownReasonNotApplicable = "N/A";
  absl::optional<MetricData> metric_data;

  const auto& boot_performance_result = result->boot_performance_result;
  if (!boot_performance_result.is_null()) {
    switch (boot_performance_result->which()) {
      case cros_healthd::BootPerformanceResult::Tag::kError: {
        DVLOG(1) << "cros_healthd: Error getting Boot Performance info: "
                 << boot_performance_result->get_error()->msg;
        break;
      }

      case cros_healthd::BootPerformanceResult::Tag::kBootPerformanceInfo: {
        const auto& boot_performance_info =
            boot_performance_result->get_boot_performance_info();
        if (boot_performance_info.is_null()) {
          DVLOG(1) << "Null BootPerformanceInfo from cros_healthd";
          break;
        }

        metric_data = absl::make_optional<MetricData>();
        auto* const boot_info_out = metric_data->mutable_telemetry_data()
                                        ->mutable_boot_performance_telemetry();
        // Gather boot performance info.
        boot_info_out->set_boot_up_seconds(
            (int64_t)boot_performance_info->boot_up_seconds);
        boot_info_out->set_boot_up_timestamp_seconds(
            (int64_t)boot_performance_info->boot_up_timestamp);
        if (boot_performance_info->shutdown_reason !=
            kShutdownReasonNotApplicable) {
          boot_info_out->set_shutdown_seconds(
              (int64_t)boot_performance_info->shutdown_seconds);
          boot_info_out->set_shutdown_timestamp_seconds(
              (int64_t)boot_performance_info->shutdown_timestamp);
        }
        boot_info_out->set_shutdown_reason(
            boot_performance_info->shutdown_reason);
        break;
      }
    }
  }

  std::move(callback).Run(metric_data);
}

void HandleAudioResult(OptionalMetricCallback callback,
                       CrosHealthdMetricSampler::MetricType metric_type,
                       cros_healthd::TelemetryInfoPtr result) {
  absl::optional<MetricData> metric_data;
  const auto& audio_result = result->audio_result;

  if (!audio_result.is_null()) {
    switch (audio_result->which()) {
      case cros_healthd::AudioResult::Tag::kError: {
        DVLOG(1) << "CrosHealthD: Error getting audio telemetry: "
                 << audio_result->get_error()->msg;
        break;
      }

      case cros_healthd::AudioResult::Tag::kAudioInfo: {
        const auto& audio_info = audio_result->get_audio_info();
        if (audio_info.is_null()) {
          DVLOG(1) << "CrosHealthD: No audio info received";
          break;
        }

        if (metric_type == CrosHealthdMetricSampler::MetricType::kTelemetry) {
          metric_data = absl::make_optional<MetricData>();
          auto* const audio_info_out =
              metric_data->mutable_telemetry_data()->mutable_audio_telemetry();
          audio_info_out->set_output_mute(audio_info->output_mute);
          audio_info_out->set_input_mute(audio_info->input_mute);
          audio_info_out->set_output_volume(audio_info->output_volume);
          audio_info_out->set_output_device_name(
              audio_info->output_device_name);
          audio_info_out->set_input_gain(audio_info->input_gain);
          audio_info_out->set_input_device_name(audio_info->input_device_name);
        }
        break;
      }
    }
  }

  std::move(callback).Run(std::move(metric_data));
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

void HandleInputResult(OptionalMetricCallback callback,
                       CrosHealthdMetricSampler::MetricType metric_type,
                       cros_healthd::TelemetryInfoPtr result) {
  absl::optional<MetricData> metric_data;
  const auto& input_result = result->input_result;

  if (!input_result.is_null()) {
    switch (input_result->which()) {
      case cros_healthd::InputResult::Tag::kError: {
        DVLOG(1) << "cros_healthd: Error getting input info: "
                 << input_result->get_error()->msg;
        break;
      }

      case cros_healthd::InputResult::Tag::kInputInfo: {
        const auto& input_info = input_result->get_input_info();
        if (input_info.is_null()) {
          DVLOG(1) << "Null InputInfo from cros_healthd";
          break;
        }

        // Gather touch screen info.
        if (metric_type == CrosHealthdMetricSampler::MetricType::kInfo) {
          metric_data = absl::make_optional<MetricData>();
          auto* const touch_screen_info_out =
              metric_data->mutable_info_data()->mutable_touch_screen_info();

          touch_screen_info_out->set_library_name(
              input_info->touchpad_library_name);

          for (const auto& screen : input_info->touchscreen_devices) {
            if (screen->input_device->is_enabled &&
                screen->input_device->connection_type ==
                    cros_healthd::InputDevice::ConnectionType::kInternal) {
              auto* const touch_screen_device_out =
                  touch_screen_info_out->add_touch_screen_devices();
              touch_screen_device_out->set_display_name(
                  screen->input_device->name);
              touch_screen_device_out->set_touch_points(screen->touch_points);
              touch_screen_device_out->set_has_stylus(screen->has_stylus);
            }
          }
          // Don't report anything if no internal touchscreen was detected.
          if (touch_screen_info_out->touch_screen_devices().empty()) {
            metric_data = absl::nullopt;
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
      HandleAudioResult(std::move(callback), metric_type, std::move(result));
      break;
    }
    case cros_healthd::ProbeCategoryEnum::kBus: {
      HandleBusResult(std::move(callback), metric_type, std::move(result));
      break;
    }
    case cros_healthd::ProbeCategoryEnum::kCpu: {
      HandleCpuResult(std::move(callback), metric_type, std::move(result));
      break;
    }
    case cros_healthd::ProbeCategoryEnum::kMemory: {
      HandleMemoryResult(std::move(callback), metric_type, std::move(result));
      break;
    }
    case cros_healthd::ProbeCategoryEnum::kBootPerformance: {
      HandleBootPerformanceResult(std::move(callback), metric_type,
                                  std::move(result));
      break;
    }
    case cros_healthd::ProbeCategoryEnum::kInput: {
      HandleInputResult(std::move(callback), metric_type, std::move(result));
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
  ash::cros_healthd::ServiceConnection::GetInstance()->ProbeTelemetryInfo(
      std::vector<cros_healthd::ProbeCategoryEnum>{probe_category_},
      std::move(healthd_callback));
}

}  // namespace reporting
