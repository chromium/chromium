// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/cros_healthd_metric_sampler.h"

#include "base/logging.h"
#include "chromeos/services/cros_healthd/public/cpp/service_connection.h"

namespace cros_healthd = chromeos::cros_healthd::mojom;

namespace reporting {
namespace {

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

void HandleBusResult(MetricCallback callback,
                     CrosHealthdMetricSampler::MetricType metric_type,
                     MetricData metric_data,
                     cros_healthd::TelemetryInfoPtr result) {
  bool anything_reported = false;
  const auto& bus_result = result->bus_result;

  if (!bus_result.is_null()) {
    switch (bus_result->which()) {
      case cros_healthd::BusResult::Tag::ERROR: {
        DVLOG(1) << "cros_healthd: Error getting bus info: "
                 << bus_result->get_error()->msg;
        break;
      }

      case cros_healthd::BusResult::Tag::BUS_DEVICES: {
        for (const auto& bus_device : bus_result->get_bus_devices()) {
          const auto& bus_info = bus_device->bus_info;
          if (bus_info->is_thunderbolt_bus_info()) {
            if (metric_type == CrosHealthdMetricSampler::MetricType::kInfo) {
              auto* const thunderbolt_info_out =
                  metric_data.mutable_info_data()
                      ->mutable_bus_device_info()
                      ->mutable_thunderbolt_info();
              anything_reported = true;
              thunderbolt_info_out->set_security_level(
                  TranslateThunderboltSecurityLevel(
                      bus_info->get_thunderbolt_bus_info()->security_level));
            }
          }
        }
        break;
      }
    }
  }

  if (anything_reported) {
    std::move(callback).Run(std::move(metric_data));
  }
}

void HandleCpuResult(MetricCallback callback,
                     CrosHealthdMetricSampler::MetricType metric_type,
                     MetricData metric_data,
                     cros_healthd::TelemetryInfoPtr result) {
  bool anything_reported = false;
  const auto& cpu_result = result->cpu_result;

  if (!cpu_result.is_null()) {
    switch (cpu_result->which()) {
      case cros_healthd::CpuResult::Tag::ERROR: {
        DVLOG(1) << "cros_healthd: Error getting CPU info: "
                 << cpu_result->get_error()->msg;
        break;
      }

      case cros_healthd::CpuResult::Tag::CPU_INFO: {
        const auto& cpu_info = cpu_result->get_cpu_info();
        if (cpu_info.is_null()) {
          DVLOG(1) << "Null CpuInfo from cros_healthd";
          break;
        }

        // Gather keylocker info.
        if (metric_type == CrosHealthdMetricSampler::MetricType::kInfo) {
          auto* const keylocker_info_out = metric_data.mutable_info_data()
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
          anything_reported = true;
        }
        break;
      }
    }
  }

  if (anything_reported) {
    std::move(callback).Run(std::move(metric_data));
  }
}

void HandleAudioResult(MetricCallback callback,
                       CrosHealthdMetricSampler::MetricType metric_type,
                       MetricData metric_data,
                       chromeos::cros_healthd::mojom::TelemetryInfoPtr result) {
  bool anything_reported = false;
  auto* const audio_info_out =
      metric_data.mutable_telemetry_data()->mutable_audio_telemetry();
  const auto& audio_result = result->audio_result;

  if (!audio_result.is_null()) {
    switch (audio_result->which()) {
      case chromeos::cros_healthd::mojom::AudioResult::Tag::ERROR: {
        DVLOG(1) << "CrosHealthD: Error getting audio telemetry: "
                 << audio_result->get_error()->msg;
        break;
      }

      case chromeos::cros_healthd::mojom::AudioResult::Tag::AUDIO_INFO: {
        const auto& audio_info = audio_result->get_audio_info();
        if (audio_info.is_null()) {
          DVLOG(1) << "CrosHealthD: No audio info received";
          break;
        }

        if (metric_type == CrosHealthdMetricSampler::MetricType::kTelemetry) {
          audio_info_out->set_output_mute(audio_info->output_mute);
          audio_info_out->set_input_mute(audio_info->input_mute);
          audio_info_out->set_output_volume(audio_info->output_volume);
          audio_info_out->set_output_device_name(
              audio_info->output_device_name);
          audio_info_out->set_input_gain(audio_info->input_gain);
          audio_info_out->set_input_device_name(audio_info->input_device_name);
          anything_reported = true;
        }
        break;
      }
    }
  }

  if (anything_reported) {
    std::move(callback).Run(std::move(metric_data));
  }
}

void HandleMemoryResult(MetricCallback callback,
                        CrosHealthdMetricSampler::MetricType metric_type,
                        MetricData metric_data,
                        cros_healthd::TelemetryInfoPtr result) {
  bool anything_reported = false;
  const auto& memory_result = result->memory_result;

  if (!memory_result.is_null()) {
    switch (memory_result->which()) {
      case cros_healthd::MemoryResult::Tag::ERROR: {
        DVLOG(1) << "cros_healthd: Error getting memory info: "
                 << memory_result->get_error()->msg;
        break;
      }

      case cros_healthd::MemoryResult::Tag::MEMORY_INFO: {
        const auto& memory_info = memory_result->get_memory_info();
        if (memory_result.is_null()) {
          DVLOG(1) << "Null MemoryInfo from cros_healthd";
          break;
        }

        // Gather memory info.
        if (metric_type == CrosHealthdMetricSampler::MetricType::kInfo) {
          auto* const memory_encryption_info_out =
              metric_data.mutable_info_data()
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
          anything_reported = true;
        }
        break;
      }
    }
  }

  if (anything_reported) {
    std::move(callback).Run(std::move(metric_data));
  }
}

void HandleNetworkResult(MetricCallback callback,
                         CrosHealthdMetricSampler::MetricType metric_type,
                         MetricData metric_data,
                         cros_healthd::TelemetryInfoPtr result) {
  const auto& network_result = result->network_interface_result;
  if (network_result.is_null()) {
    std::move(callback).Run(metric_data);
    return;
  }

  switch (network_result->which()) {
    case cros_healthd::NetworkInterfaceResult::Tag::ERROR: {
      DVLOG(1) << "cros_healthd: Error getting network result: "
               << network_result->get_error()->msg;
      break;
    }

    case cros_healthd::NetworkInterfaceResult::Tag::NETWORK_INTERFACE_INFO: {
      for (const auto& network_info :
           network_result->get_network_interface_info()) {
        // Handle wireless interface telemetry
        if (network_info->is_wireless_interface_info() &&
            metric_type == CrosHealthdMetricSampler::MetricType::kTelemetry) {
          auto* network_telemetry_list = metric_data.mutable_telemetry_data()
                                             ->mutable_networks_telemetry();
          ::reporting::NetworkTelemetry* network_telemetry_out;

          // Find if wireless telemetry already exists in metric data.
          for (int i = 0; i < network_telemetry_list->network_telemetry_size();
               i++) {
            if (network_telemetry_list->network_telemetry(i).type() ==
                ::reporting::NetworkType::WIFI) {
              network_telemetry_out =
                  network_telemetry_list->mutable_network_telemetry(i);
            }
          }
          if (!network_telemetry_out) {
            network_telemetry_out =
                network_telemetry_list->add_network_telemetry();
          }

          // Set data.
          auto* const interface_telemetry_out =
              network_telemetry_out->add_network_interface_telemetry();
          auto* const wireless_telemetry_out =
              interface_telemetry_out->mutable_wireless_interface();
          const auto& wireless_info =
              network_info->get_wireless_interface_info();

          interface_telemetry_out->set_interface_name(
              wireless_info->interface_name);
          wireless_telemetry_out->set_power_management_enabled(
              wireless_info->power_management_on);

          if (wireless_info->wireless_link_info) {
            const auto& wireless_link_info = wireless_info->wireless_link_info;
            wireless_telemetry_out->set_access_point_address(
                wireless_link_info->access_point_address_str);
            wireless_telemetry_out->set_tx_bit_rate_mbps(
                wireless_link_info->tx_bit_rate_mbps);
            wireless_telemetry_out->set_rx_bit_rate_mbps(
                wireless_link_info->rx_bit_rate_mbps);
            wireless_telemetry_out->set_tx_power_dbm(
                wireless_link_info->tx_power_dBm);
            wireless_telemetry_out->set_encryption_on(
                wireless_link_info->encyption_on);
            wireless_telemetry_out->set_link_quality(
                wireless_link_info->link_quality);
            wireless_telemetry_out->set_signal_level_dbm(
                wireless_link_info->signal_level_dBm);
          }
        }
      }
    }
  }

  std::move(callback).Run(std::move(metric_data));
}

void OnHealthdInfoReceived(MetricCallback callback,
                           cros_healthd::ProbeCategoryEnum probe_category,
                           CrosHealthdMetricSampler::MetricType metric_type,
                           MetricData metric_data,
                           cros_healthd::TelemetryInfoPtr result) {
  DCHECK(result);

  switch (probe_category) {
    case cros_healthd::ProbeCategoryEnum::kAudio: {
      HandleAudioResult(std::move(callback), metric_type,
                        std::move(metric_data), std::move(result));
      break;
    }
    case cros_healthd::ProbeCategoryEnum::kBus: {
      HandleBusResult(std::move(callback), metric_type, std::move(metric_data),
                      std::move(result));
      break;
    }
    case cros_healthd::ProbeCategoryEnum::kCpu: {
      HandleCpuResult(std::move(callback), metric_type, std::move(metric_data),
                      std::move(result));
      break;
    }
    case cros_healthd::ProbeCategoryEnum::kMemory: {
      HandleMemoryResult(std::move(callback), metric_type,
                         std::move(metric_data), std::move(result));
      break;
    }
    case cros_healthd::ProbeCategoryEnum::kNetworkInterface: {
      HandleNetworkResult(std::move(callback), metric_type,
                          std::move(metric_data), std::move(result));
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

void CrosHealthdMetricSampler::Collect(MetricCallback callback) {
  auto healthd_callback =
      base::BindOnce(OnHealthdInfoReceived, std::move(callback),
                     probe_category_, metric_type_, std::move(metric_data_));
  metric_data_.Clear();
  chromeos::cros_healthd::ServiceConnection::GetInstance()->ProbeTelemetryInfo(
      std::vector<cros_healthd::ProbeCategoryEnum>{probe_category_},
      std::move(healthd_callback));
}

void CrosHealthdMetricSampler::SetMetricData(MetricData metric_data) {
  metric_data_ = std::move(metric_data);
}
}  // namespace reporting
