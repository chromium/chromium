// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/extension_telemetry/script_injection_signal_processor.h"

#include "base/check.h"
#include "chrome/browser/safe_browsing/extension_telemetry/script_injection_signal.h"

namespace safe_browsing {

namespace {
// Max number of unique signals stored per extension.
constexpr size_t kMaxAggregatedSignals = 100;
}  // namespace

ScriptInjectionSignalProcessor::ScriptInjectionData::ScriptInjectionData() =
    default;
ScriptInjectionSignalProcessor::ScriptInjectionData::~ScriptInjectionData() =
    default;
ScriptInjectionSignalProcessor::ScriptInjectionData::ScriptInjectionData(
    const ScriptInjectionData&) = default;

ScriptInjectionSignalProcessor::ScriptInjectionStoreEntry::
    ScriptInjectionStoreEntry() = default;
ScriptInjectionSignalProcessor::ScriptInjectionStoreEntry::
    ~ScriptInjectionStoreEntry() = default;
ScriptInjectionSignalProcessor::ScriptInjectionStoreEntry::
    ScriptInjectionStoreEntry(const ScriptInjectionStoreEntry&) = default;

ScriptInjectionSignalProcessor::ScriptInjectionSignalProcessor()
    : max_aggregated_signals_(kMaxAggregatedSignals) {}
ScriptInjectionSignalProcessor::~ScriptInjectionSignalProcessor() = default;

void ScriptInjectionSignalProcessor::ProcessSignal(
    const ExtensionSignal& signal) {
  DCHECK_EQ(signal.GetType(), ExtensionSignalType::kScriptInjection);
  const auto& script_injection_signal =
      static_cast<const ScriptInjectionSignal&>(signal);

  ScriptInjectionStoreEntry& store_entry =
      script_injection_store_[script_injection_signal.extension_id()];
  ScriptInjectionDataMap& data_map = store_entry.script_injection_data_map;

  std::string aggregation_key = script_injection_signal.GetAggregationKey();
  auto it = data_map.find(aggregation_key);
  if (it != data_map.end()) {
    it->second.count++;
    if (script_injection_signal.timestamp() > it->second.last_timestamp) {
      it->second.last_timestamp = script_injection_signal.timestamp();
    }
  } else if (data_map.size() < max_aggregated_signals_) {
    ScriptInjectionData data;
    data.api_name = script_injection_signal.api_name();
    data.url = script_injection_signal.url();
    data.args_list = script_injection_signal.args_list();
    data.arg_url = script_injection_signal.arg_url();
    data.last_timestamp = script_injection_signal.timestamp();
    data.count = 1;
    data_map.emplace(std::move(aggregation_key), std::move(data));
  }
}

std::unique_ptr<ExtensionTelemetryReportRequest_SignalInfo>
ScriptInjectionSignalProcessor::GetSignalInfoForReport(
    const extensions::ExtensionId& extension_id) {
  auto it = script_injection_store_.find(extension_id);
  if (it == script_injection_store_.end()) {
    return nullptr;
  }

  auto signal_info =
      std::make_unique<ExtensionTelemetryReportRequest_SignalInfo>();
  auto* script_injection_info = signal_info->mutable_script_injection_info();

  for (auto& [key, data] : it->second.script_injection_data_map) {
    auto* script_injection = script_injection_info->add_script_injections();
    script_injection->set_api_name(std::move(data.api_name));
    script_injection->set_url(std::move(data.url));
    for (auto& arg : data.args_list) {
      script_injection->add_args_list(std::move(arg));
    }
    script_injection->set_arg_url(std::move(data.arg_url));
    script_injection->set_timestamp_ms(
        data.last_timestamp.InMillisecondsSinceUnixEpoch());
    script_injection->set_count(data.count);
  }

  script_injection_store_.erase(it);
  return signal_info;
}

bool ScriptInjectionSignalProcessor::HasDataToReportForTest() const {
  return !script_injection_store_.empty();
}

void ScriptInjectionSignalProcessor::SetMaxAggregatedSignalsForTest(
    size_t max_aggregated_signals) {
  max_aggregated_signals_ = max_aggregated_signals;
}

}  // namespace safe_browsing
