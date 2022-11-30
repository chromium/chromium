// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/extension_telemetry/tabs_execute_script_signal_processor.h"

#include "base/check_op.h"
#include "chrome/browser/safe_browsing/extension_telemetry/tabs_execute_script_signal.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"

namespace safe_browsing {

// Used to limit the number of unique script hashes stored for each extension.
const size_t kMaxScriptHashes = 100;

TabsExecuteScriptSignalProcessor::ScriptHashStoreEntry::ScriptHashStoreEntry() =
    default;
TabsExecuteScriptSignalProcessor::ScriptHashStoreEntry::
    ~ScriptHashStoreEntry() = default;
TabsExecuteScriptSignalProcessor::ScriptHashStoreEntry::ScriptHashStoreEntry(
    const ScriptHashStoreEntry& src) = default;
TabsExecuteScriptSignalProcessor::TabsExecuteScriptSignalProcessor()
    : max_script_hashes_(kMaxScriptHashes) {}
TabsExecuteScriptSignalProcessor::~TabsExecuteScriptSignalProcessor() = default;

void TabsExecuteScriptSignalProcessor::ProcessSignal(
    const ExtensionSignal& signal) {
  DCHECK_EQ(ExtensionSignalType::kTabsExecuteScript, signal.GetType());
  const auto& tes_signal = static_cast<const TabsExecuteScriptSignal&>(signal);
  // Note that if this is the first signal for an extension, a new entry is
  // created in the store.
  ScriptHashStoreEntry& store_entry =
      script_hash_store_[tes_signal.extension_id()];
  ScriptHashes& script_hashes = store_entry.script_hashes;
  // Only process signal if:
  // - the number of script hashes for the extension is under the max limit OR
  // - the script hash already exists in the extension's script hash list.
  if ((script_hashes.size() < max_script_hashes_) ||
      (script_hashes.find(tes_signal.script_hash()) != script_hashes.end())) {
    // Process signal - increment execution count for script hash.
    // Note that if this is a new script hash, a new entry is created.
    ++(script_hashes[tes_signal.script_hash()]);
  } else {
    // Max script hashes exceeded for this extension.
    store_entry.max_exceeded_script_count++;
  }
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
  for (auto& script_hashes_it : script_hash_store_it->second.script_hashes) {
    ExtensionTelemetryReportRequest_SignalInfo_TabsExecuteScriptInfo_ScriptInfo*
        script_pb = tabs_execute_script_info->add_scripts();
    script_pb->set_hash(std::move(script_hashes_it.first));
    script_pb->set_execution_count(script_hashes_it.second);
  }
  tabs_execute_script_info->set_max_exceeded_script_count(
      script_hash_store_it->second.max_exceeded_script_count);

  // Finally, clear the data in the script hashes store.
  script_hash_store_.erase(script_hash_store_it);

  return signal_info;
}

bool TabsExecuteScriptSignalProcessor::HasDataToReportForTest() const {
  return !script_hash_store_.empty();
}

void TabsExecuteScriptSignalProcessor::SetMaxScriptHashesForTest(
    size_t max_script_hashes) {
  max_script_hashes_ = max_script_hashes;
}

}  // namespace safe_browsing
