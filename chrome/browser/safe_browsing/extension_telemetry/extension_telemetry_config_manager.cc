// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_config_manager.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_number_conversions_win.h"
#include "base/values.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_signal.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"

namespace safe_browsing {

namespace {
// Default values for the ExtensionTelemetryConfigManager and the
// string key values for the `configuration_` dict.
constexpr const uint32_t kDefaultWritesPerInterval = 1u;
constexpr const uint32_t kDefaultReportingInterval = 3600u;
constexpr const uint32_t kDefaultConfigVersion = 0u;
constexpr const uint64_t kDefaultSignalEnables = 0xffffffffffffffff;
constexpr const char kConfigurationVersion[] = "version";
constexpr const char kWritesPerInterval[] = "writes_per_interval";
constexpr const char kReportingInterval[] = "reporting_interval";
constexpr const char kSignalEnables0[] = "signal_enables_0";
constexpr const char kSignalEnables1[] = "signal_enables_1";
}  // namespace

ExtensionTelemetryConfigManager::~ExtensionTelemetryConfigManager() = default;

ExtensionTelemetryConfigManager::ExtensionTelemetryConfigManager(
    PrefService* pref_service)
    : pref_service_(pref_service) {}

void ExtensionTelemetryConfigManager::LoadConfig() {
  configuration_ = GetExtensionTelemetryConfig(*pref_service_).Clone();
}

// A copy of the configuration data stored in prefs. The configuration
// data is organized as a dictionary (see example below).
// "safe_browsing.extension_telemetry_configuration": <- DICT
//   "version":1
//   "reporting_interval":3600
//   "writes_per_interval":1
//   "<extension_id_1>": <- DICT
//      "signal_enables_0" : 0x0000000f
//      "signal_enables_1" : 0x00000000
//   "<extension_id_2>": <- DICT
//      "signal_enables_0" : 0x0000000c
//      "signal_enables_1" : 0x00000000
void ExtensionTelemetryConfigManager::SaveConfig(
    const ExtensionTelemetryReportResponse_Configuration& telemetry_config) {
  if (!telemetry_config.has_configuration_version()) {
    return;
  }
  uint32_t configuration_version = telemetry_config.configuration_version();
  if (configuration_version <= GetConfigVersion()) {
    return;
  }
  base::Value::Dict telemetry_config_dict;
  telemetry_config_dict.Set(kConfigurationVersion,
                            static_cast<int>(configuration_version));
  if (telemetry_config.has_reporting_interval_seconds()) {
    telemetry_config_dict.Set(
        kReportingInterval,
        static_cast<int>(telemetry_config.reporting_interval_seconds()));
  }
  if (telemetry_config.has_writes_per_interval()) {
    telemetry_config_dict.Set(
        kWritesPerInterval,
        static_cast<int>(telemetry_config.writes_per_interval()));
  }
  // Extract signal enables represented as bitmasks.
  for (auto& proto_extension_parameter :
       telemetry_config.extension_parameters()) {
    uint64_t signal_enables_bitmask =
        proto_extension_parameter.signal_enable_mask();
    base::Value::Dict extension_dict;
    // In order to store the bitmask into the base::value::dict object
    // the `signal_enables` bitmask must be split from it's uint64 form
    // into two int32 variables.
    extension_dict.Set(kSignalEnables0,
                       static_cast<int>(signal_enables_bitmask));
    extension_dict.Set(kSignalEnables1,
                       static_cast<int>(signal_enables_bitmask >> 32));
    telemetry_config_dict.Set(proto_extension_parameter.extension_id(),
                              std::move(extension_dict));
  }
  SetExtensionTelemetryConfig(*pref_service_, telemetry_config_dict);
  configuration_ = std::move(telemetry_config_dict);
}

bool ExtensionTelemetryConfigManager::IsSignalEnabled(
    const extensions::ExtensionId& extension_id,
    ExtensionSignalType signal_type) const {
  auto* extension_dict = configuration_.FindDict(extension_id);
  if (!extension_dict) {
    return true;
  }
  // Construct the uint64 `signal_enables` bitmask from the uint32_t
  // `signal_enables_0` and `signal_enables_1` variables.
  uint64_t signal_enables_bitmask =
      (static_cast<uint64_t>(*extension_dict->FindInt(kSignalEnables1)) << 32 |
       static_cast<uint32_t>(*extension_dict->FindInt(kSignalEnables0)));
  return (signal_enables_bitmask & (1 << (static_cast<int>(signal_type))));
}

uint32_t ExtensionTelemetryConfigManager::GetWritesPerInterval() const {
  absl::optional<int> param = configuration_.FindInt(kWritesPerInterval);
  int value = param.has_value() ? param.value() : kDefaultWritesPerInterval;
  return static_cast<uint32_t>(value);
}

uint32_t ExtensionTelemetryConfigManager::GetConfigVersion() const {
  absl::optional<int> param = configuration_.FindInt(kConfigurationVersion);
  int value = param.has_value() ? param.value() : kDefaultConfigVersion;
  return static_cast<uint32_t>(value);
}

uint32_t ExtensionTelemetryConfigManager::GetReportingInterval() const {
  absl::optional<int> param = configuration_.FindInt(kReportingInterval);
  int value = param.has_value() ? param.value() : kDefaultReportingInterval;
  return static_cast<uint32_t>(value);
}

uint64_t ExtensionTelemetryConfigManager::GetSignalEnables(
    const extensions::ExtensionId& extension_id) const {
  auto* extension_dict = configuration_.FindDict(extension_id);
  if (!extension_dict) {
    // By default, all signals are enabled for extensions.
    return kDefaultSignalEnables;
  }
  absl::optional<uint64_t> param =
      (static_cast<uint64_t>(*extension_dict->FindInt(kSignalEnables1)) << 32 |
       static_cast<uint32_t>(*extension_dict->FindInt(kSignalEnables0)));
  uint64_t value = param.has_value() ? param.value() : kDefaultSignalEnables;
  return value;
}
}  // namespace safe_browsing
