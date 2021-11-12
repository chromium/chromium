// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/extension_telemetry/tabs_execute_script_signal_processor.h"

#include "base/check_op.h"
#include "chrome/browser/safe_browsing/extension_telemetry/tabs_execute_script_signal.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"

namespace safe_browsing {

TabsExecuteScriptSignalProcessor::TabsExecuteScriptSignalProcessor() = default;
TabsExecuteScriptSignalProcessor::~TabsExecuteScriptSignalProcessor() = default;

void TabsExecuteScriptSignalProcessor::ProcessSignal(
    std::unique_ptr<ExtensionSignal> signal) {
  DCHECK_EQ(ExtensionSignalType::kTabsExecuteScript, signal->GetType());
  auto* tes_signal = static_cast<TabsExecuteScriptSignal*>(signal.get());
  ++((script_hash_store_[tes_signal->extension_id()])[tes_signal
                                                          ->script_hash()]);
}

std::unique_ptr<ExtensionTelemetryReportRequest_SignalInfo>
TabsExecuteScriptSignalProcessor::GetSignalInfoForReport(
    const extensions::ExtensionId& extension_id) {
  auto script_hash_store_it = script_hash_store_.find(extension_id);
  if (script_hash_store_it == script_hash_store_.end())
    return nullptr;

  // Create the signal info protobuf.
  auto signal_info =
      std::make_unique<ExtensionTelemetryReportRequest_SignalInfo>();
  ExtensionTelemetryReportRequest_SignalInfo_TabsExecuteScriptInfo*
      tabs_execute_script_info =
          signal_info->mutable_tabs_execute_script_info();
  for (auto& script_hashes_it : script_hash_store_it->second) {
    ExtensionTelemetryReportRequest_SignalInfo_TabsExecuteScriptInfo_ScriptInfo*
        script_pb = tabs_execute_script_info->add_scripts();
    script_pb->set_hash(std::move(script_hashes_it.first));
    script_pb->set_execution_count(script_hashes_it.second);
  }

  // Finally, clear the data in the script hashes store.
  script_hash_store_.erase(script_hash_store_it);

  return signal_info;
}

bool TabsExecuteScriptSignalProcessor::HasDataToReportForTest() const {
  return !script_hash_store_.empty();
}

}  // namespace safe_browsing
