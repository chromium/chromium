// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_EXTENSION_TELEMETRY_CONFIG_MANAGER_H_
#define CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_EXTENSION_TELEMETRY_CONFIG_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "extensions/common/extension_id.h"

class PrefService;

namespace safe_browsing {

enum class ExtensionSignalType;
class ExtensionTelemetryReportResponse_Configuration;

// The ExtensionTelemetryConfigManager manages the configuration of the
// Extension Telemetry Service. Configuration data includes items such as:
//   - how often telemetry reports are uploaded
//   - how often telemetry reports are persisted to disk
//   - which extensions and signals are to be included in telemetry
//     reports
// ExtensionTelemetryConfigManager persists the configuration data using
// Chrome's pref service. It also provides methods to look up this data.
// This object is instantiated and owned by the ExtensionTelemetryService
// and lives on the Browser UI thread.
class ExtensionTelemetryConfigManager {
 public:
  explicit ExtensionTelemetryConfigManager(PrefService* pref_service);

  ExtensionTelemetryConfigManager(const ExtensionTelemetryConfigManager&) =
      delete;
  ExtensionTelemetryConfigManager& operator=(
      const ExtensionTelemetryConfigManager&) = delete;

  virtual ~ExtensionTelemetryConfigManager();

  // Loads telemetry configuration data from prefs into its own internal
  // copy. If there is no configuration data present, the internal copy
  // becomes/remains empty. All configuration getters return default
  // values.
  void LoadConfig();

  // The telemetry configuration is sent by a telemetry server in response
  // to an uploaded telemetry report. The ExtensionTelemetryService calls
  // this method to pass the response protobuf to the
  // ExtensionTelemetryConfigManager. If the protobuf has a newer
  // version than the currently saved config, it extracts the
  // configuration data and persists it in Chrome's pref service.
  // A copy of the configuration data is also kept in an internal member.
  void SaveConfig(
      const ExtensionTelemetryReportResponse_Configuration& telemetry_config);

  // Determines if the specified signal_type should be processed for the
  // specified extension. Returns true if no configuration data has been
  // saved.
  bool IsSignalEnabled(const extensions::ExtensionId& extension_id,
                       ExtensionSignalType signal_type) const;

  // Returns the writes per interval determined by the current
  // configuration or a default value if no configuration data has been
  // saved.
  uint32_t GetWritesPerInterval() const;

  // Returns the reporting interval determined by the current
  // configuration or a default value if no configuration data has been
  // saved.
  uint32_t GetReportingInterval() const;

  // Returns the config version determined by the current
  // configuration or a default value if no configuration data has been
  // saved.
  uint32_t GetConfigVersion() const;

  // Returns the signal enables bitmask for the `extension_id` determined
  // by the current configuration or a default value if no configuration
  // data has been saved.
  uint64_t GetSignalEnables(const extensions::ExtensionId& extension_id) const;

 private:
  // Holds string values that map to the configurable Extension Telemetry
  // Service variables. Packed into a base::Value::Dict so it can be
  // stored in Chrome prefs.
  base::Value::Dict configuration_;

  // ExtensionTelemetryConfigManager uses the pref service to store
  // the telemetry config.
  raw_ptr<PrefService> pref_service_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_EXTENSION_TELEMETRY_CONFIG_MANAGER_H_
