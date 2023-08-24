// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/extension_telemetry/tabs_api_signal_processor.h"
#include <string>

#include "base/check_op.h"
#include "chrome/browser/safe_browsing/extension_telemetry/tabs_api_signal.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"

namespace safe_browsing {

namespace {

// Used to limit the number of unique API call details stored for each
// extension.
constexpr size_t kMaxUniqueCallDetails = 100;

}  // namespace

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
  TabsApiCallDetailsMap& call_details_map =
      info_store_entry.tabs_api_call_details_map;

  const std::string call_details_id = tabs_api_signal.GetUniqueCallDetailsId();
  auto call_details_it = call_details_map.find(call_details_id);
  if (call_details_it != call_details_map.end()) {
    // If a tabs API call with the same arguments has been invoked
    // before, simply increment the count for the corresponding record.
    auto count = call_details_it->second.count();
    call_details_it->second.set_count(count + 1);
  } else if (call_details_map.size() < max_unique_call_details_) {
    // For new call details, process the signal only if under max limit, i.e,
    // add a new TabsApiCallDetails object to the call details map.
    TabsApiCallDetails call_details;
    call_details.set_method(tabs_api_signal.api_method());
    call_details.set_current_url(tabs_api_signal.current_url());
    call_details.set_new_url(tabs_api_signal.new_url());
    call_details.set_count(1);
    call_details_map.emplace(call_details_id, call_details);
  }
}

std::unique_ptr<ExtensionTelemetryReportRequest_SignalInfo>
TabsApiSignalProcessor::GetSignalInfoForReport(
    const extensions::ExtensionId& extension_id) {
  auto tabs_api_info_store_entry = tabs_api_info_store_.find(extension_id);
  if (tabs_api_info_store_entry == tabs_api_info_store_.end()) {
    return nullptr;
  }

  // Create the signal info protobuf.
  auto signal_info =
      std::make_unique<ExtensionTelemetryReportRequest_SignalInfo>();
  TabsApiInfo* tabs_api_info = signal_info->mutable_tabs_api_info();

  for (auto& call_details :
       tabs_api_info_store_entry->second.tabs_api_call_details_map) {
    *tabs_api_info->add_call_details() = std::move(call_details.second);
  }

  // Finally, clear the data in the info store.
  tabs_api_info_store_.erase(tabs_api_info_store_entry);

  return signal_info;
}

bool TabsApiSignalProcessor::HasDataToReportForTest() const {
  return !tabs_api_info_store_.empty();
}

void TabsApiSignalProcessor::SetMaxUniqueCallDetailsForTest(
    size_t max_unique_call_details) {
  max_unique_call_details_ = max_unique_call_details;
}

}  // namespace safe_browsing
