// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/extension_telemetry/tabs_api_signal_processor.h"

#include <string>

#include "base/check_op.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/safe_browsing/extension_telemetry/tabs_api_signal.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"

namespace safe_browsing {

namespace {

// Used to limit the number of unique API call details stored for each
// extension.
constexpr size_t kMaxUniqueCallDetails = 100;

}  // namespace

TabsApiSignalProcessor::CallData::CallData() = default;
TabsApiSignalProcessor::CallData::~CallData() = default;
TabsApiSignalProcessor::CallData::CallData(const CallData& src) = default;

TabsApiSignalProcessor::TabsApiInfoStoreEntry::TabsApiInfoStoreEntry() =
    default;
TabsApiSignalProcessor::TabsApiInfoStoreEntry::~TabsApiInfoStoreEntry() =
    default;
TabsApiSignalProcessor::TabsApiInfoStoreEntry::TabsApiInfoStoreEntry(
    const TabsApiInfoStoreEntry& src) = default;
TabsApiSignalProcessor::TabsApiSignalProcessor()
    : max_unique_call_details_(kMaxUniqueCallDetails) {}
TabsApiSignalProcessor::~TabsApiSignalProcessor() = default;

void TabsApiSignalProcessor::ProcessSignal(const ExtensionSignal& signal) {
  // Validate TabsApi signal.
  DCHECK_EQ(ExtensionSignalType::kTabsApi, signal.GetType());
  const auto& tabs_api_signal = static_cast<const TabsApiSignal&>(signal);
  // Ignore the signal if both URL (current and new) are empty (sanity check).
  if (tabs_api_signal.current_url().empty() &&
      tabs_api_signal.new_url().empty()) {
    return;
  }

  // Retrieve the signal info store entry for this extension from the store. If
  // this is the first signal for an extension, a new entry is created in the
  // store.
  TabsApiInfoStoreEntry& info_store_entry =
      tabs_api_info_store_[tabs_api_signal.extension_id()];
  CallDataMap& call_data_map = info_store_entry.call_data_map;

  std::string call_details_id = tabs_api_signal.GetUniqueCallDetailsId();
  auto call_data_it = call_data_map.find(call_details_id);
  if (call_data_it != call_data_map.end()) {
    // If a tabs API call with the same arguments has been invoked
    // before, simply increment the count for the corresponding record.
    auto& [call_details, js_callstacks] = call_data_it->second;
    auto count = call_details.count();
    call_details.set_count(count + 1);
    js_callstacks.Add(tabs_api_signal.js_callstack());
  } else if (call_data_map.size() < max_unique_call_details_) {
    // For new call details, process the signal only if under max limit, i.e,
    // add a new entry in the call data map.
    CallData call_data;
    call_data.call_details.set_method(tabs_api_signal.api_method());
    call_data.call_details.set_current_url(tabs_api_signal.current_url());
    call_data.call_details.set_new_url(tabs_api_signal.new_url());
    call_data.call_details.set_count(1);
    call_data.js_callstacks.Add(tabs_api_signal.js_callstack());
    call_data_map.emplace(std::move(call_details_id), std::move(call_data));
  }
}

std::unique_ptr<ExtensionTelemetryReportRequest_SignalInfo>
TabsApiSignalProcessor::GetSignalInfoForReport(
    const extensions::ExtensionId& extension_id) {
  auto tabs_api_info_store_it = tabs_api_info_store_.find(extension_id);
  if (tabs_api_info_store_it == tabs_api_info_store_.end()) {
    return nullptr;
  }

  // Create the signal info protobuf.
  auto signal_info_pb =
      std::make_unique<ExtensionTelemetryReportRequest_SignalInfo>();
  TabsApiInfo* tabs_api_info_pb = signal_info_pb->mutable_tabs_api_info();

  for (auto& [key, call_info] : tabs_api_info_store_it->second.call_data_map) {
    // Get the JS call stacks associated with this call details
    // and add them to the CallDetails message.
    auto js_callstacks = call_info.js_callstacks.GetAll();
    call_info.call_details.mutable_js_callstacks()->Assign(
        js_callstacks.begin(), js_callstacks.end());
    // Add the CallDetails message to the signal info object.
    *tabs_api_info_pb->add_call_details() = std::move(call_info.call_details);
  }

  // Finally, clear the data in the info store.
  tabs_api_info_store_.erase(tabs_api_info_store_it);

  return signal_info_pb;
}

bool TabsApiSignalProcessor::HasDataToReportForTest() const {
  return !tabs_api_info_store_.empty();
}

void TabsApiSignalProcessor::SetMaxUniqueCallDetailsForTest(
    size_t max_unique_call_details) {
  max_unique_call_details_ = max_unique_call_details;
}

}  // namespace safe_browsing
