// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_SCRIPT_INJECTION_SIGNAL_PROCESSOR_H_
#define CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_SCRIPT_INJECTION_SIGNAL_PROCESSOR_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/time/time.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_signal_processor.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"

namespace safe_browsing {

class ExtensionSignal;
class ExtensionTelemetryReportRequest_SignalInfo;

// A class that processes script injection signal data to generate telemetry
// reports.
class ScriptInjectionSignalProcessor : public ExtensionSignalProcessor {
 public:
  ScriptInjectionSignalProcessor();
  ~ScriptInjectionSignalProcessor() override;

  ScriptInjectionSignalProcessor(ScriptInjectionSignalProcessor&) = delete;
  ScriptInjectionSignalProcessor& operator=(ScriptInjectionSignalProcessor&) =
      delete;

  // ExtensionSignalProcessor:
  void ProcessSignal(const ExtensionSignal& signal) override;
  std::unique_ptr<ExtensionTelemetryReportRequest_SignalInfo>
  GetSignalInfoForReport(const extensions::ExtensionId& extension_id) override;
  bool HasDataToReportForTest() const override;

  void SetMaxAggregatedSignalsForTest(size_t max_aggregated_signals);

 protected:
  // Max number of unique signals stored per extension.
  size_t max_aggregated_signals_;

  struct ScriptInjectionData {
    ScriptInjectionData();
    ~ScriptInjectionData();
    ScriptInjectionData(const ScriptInjectionData&);

    std::string api_name;
    std::string url;
    std::vector<std::string> args_list;
    std::string arg_url;
    base::Time last_timestamp;
    uint32_t count = 0;
  };

  // Stores a map of unique signals, keyed by their aggregation key.
  using ScriptInjectionDataMap =
      base::flat_map<std::string, ScriptInjectionData>;

  struct ScriptInjectionStoreEntry {
    ScriptInjectionStoreEntry();
    ~ScriptInjectionStoreEntry();
    ScriptInjectionStoreEntry(const ScriptInjectionStoreEntry&);

    ScriptInjectionDataMap script_injection_data_map;
  };

  using ScriptInjectionStore =
      base::flat_map<extensions::ExtensionId, ScriptInjectionStoreEntry>;
  ScriptInjectionStore script_injection_store_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_SCRIPT_INJECTION_SIGNAL_PROCESSOR_H_
