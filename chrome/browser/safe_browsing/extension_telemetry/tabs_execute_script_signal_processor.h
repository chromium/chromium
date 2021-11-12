// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_TABS_EXECUTE_SCRIPT_SIGNAL_PROCESSOR_H_
#define CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_TABS_EXECUTE_SCRIPT_SIGNAL_PROCESSOR_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_signal_processor.h"

namespace safe_browsing {

class ExtensionSignal;
class ExtensionTelemetryReportRequest_SignalInfo;

// A class that processes tabs.executeScript signal data to generate telemetry
// reports.
class TabsExecuteScriptSignalProcessor : public ExtensionSignalProcessor {
 public:
  TabsExecuteScriptSignalProcessor();
  ~TabsExecuteScriptSignalProcessor() override;

  TabsExecuteScriptSignalProcessor(TabsExecuteScriptSignalProcessor&) = delete;
  TabsExecuteScriptSignalProcessor& operator=(
      const TabsExecuteScriptSignalProcessor&) = delete;

  // ExtensionSignalProcessor:
  void ProcessSignal(std::unique_ptr<ExtensionSignal> signal) override;
  std::unique_ptr<ExtensionTelemetryReportRequest_SignalInfo>
  GetSignalInfoForReport(const extensions::ExtensionId& extension_id) override;
  bool HasDataToReportForTest() const override;

 protected:
  // Maps script hash to execution count.
  using ScriptHashes = base::flat_map<std::string, uint32_t>;
  // Maps extension id to (script, execution count).
  using ScriptHashesPerExtension =
      base::flat_map<extensions::ExtensionId, ScriptHashes>;
  ScriptHashesPerExtension script_hash_store_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_TABS_EXECUTE_SCRIPT_SIGNAL_PROCESSOR_H_
