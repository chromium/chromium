// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_config_manager.h"

#include "base/values.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_signal.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"

namespace safe_browsing {

namespace {
// Default values for the ExtensionTelemetryConfigManager and the
// string key values for the `configuration_` dict.
constexpr uint32_t kDefaultWritesPerInterval = 1u;
constexpr uint32_t kDefaultReportingInterval = 3600u;
constexpr uint32_t kDefaultConfigVersion = 0u;
constexpr uint32_t kDefaultSignalEnables32 = 0xffffffff;
constexpr uint64_t kDefaultSignalEnables = 0xffffffffffffffff;
constexpr char kConfigurationVersion[] = "version";
constexpr char kWritesPerInterval[] = "writes_per_interval";
constexpr char kReportingInterval[] = "reporting_interval";
constexpr char kSignalEnables0[] = "signal_enables_0";
constexpr char kSignalEnables1[] = "signal_enables_1";
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
  const base::Value::Dict* extension_dict =
      configuration_.FindDict(extension_id);
  if (!extension_dict) {
    return true;
  }
  uint64_t signal_enables_bitmask = GetSignalEnables(extension_id);
  return (signal_enables_bitmask &
          (1ull << (static_cast<uint64_t>(signal_type))));
}

uint32_t ExtensionTelemetryConfigManager::GetWritesPerInterval() const {
  std::optional<int> param = configuration_.FindInt(kWritesPerInterval);
  return static_cast<uint32_t>(param.value_or(kDefaultWritesPerInterval));
}

uint32_t ExtensionTelemetryConfigManager::GetConfigVersion() const {
  std::optional<int> param = configuration_.FindInt(kConfigurationVersion);
  return static_cast<uint32_t>(param.value_or(kDefaultConfigVersion));
}

uint32_t ExtensionTelemetryConfigManager::GetReportingInterval() const {
  std::optional<int> param = configuration_.FindInt(kReportingInterval);
  return static_cast<uint32_t>(param.value_or(kDefaultReportingInterval));
}

uint64_t ExtensionTelemetryConfigManager::GetSignalEnables(
    const extensions::ExtensionId& extension_id) const {
  const base::Value::Dict* extension_dict =
      configuration_.FindDict(extension_id);
  if (!extension_dict) {
    // By default, all signals are enabled for extensions.
    return kDefaultSignalEnables;
  }
  uint32_t signal_enables_0 =
      static_cast<uint32_t>(extension_dict->FindInt(kSignalEnables0)
                                .value_or(kDefaultSignalEnables32));
  uint32_t signal_enables_1 =
      static_cast<uint32_t>(extension_dict->FindInt(kSignalEnables1)
                                .value_or(kDefaultSignalEnables32));
  return static_cast<uint64_t>(signal_enables_1) << 32 | signal_enables_0;
}
}  // namespace safe_browsing
