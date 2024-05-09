// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/extension_telemetry/declarative_net_request_action_signal_processor.h"

#include "base/check_op.h"
#include "chrome/browser/safe_browsing/extension_telemetry/declarative_net_request_action_signal.h"

namespace safe_browsing {

namespace {

// Used to limit the number of unique action details stored for each
// extension.
constexpr size_t kMaxUniqueActionDetails = 100;

}  // namespace

DeclarativeNetRequestActionSignalProcessor::
    DeclarativeNetRequestActionInfoStoreEntry::
        DeclarativeNetRequestActionInfoStoreEntry() = default;
DeclarativeNetRequestActionSignalProcessor::
    DeclarativeNetRequestActionInfoStoreEntry::
        ~DeclarativeNetRequestActionInfoStoreEntry() = default;
DeclarativeNetRequestActionSignalProcessor::
    DeclarativeNetRequestActionInfoStoreEntry::
        DeclarativeNetRequestActionInfoStoreEntry(
            const DeclarativeNetRequestActionInfoStoreEntry& src) = default;
DeclarativeNetRequestActionSignalProcessor::
    DeclarativeNetRequestActionSignalProcessor()
    : max_unique_action_details_(kMaxUniqueActionDetails) {}
DeclarativeNetRequestActionSignalProcessor::
    ~DeclarativeNetRequestActionSignalProcessor() = default;

void DeclarativeNetRequestActionSignalProcessor::ProcessSignal(
    const ExtensionSignal& signal) {
  // Validate DeclarativeNetRequestAction signal.
  DCHECK_EQ(ExtensionSignalType::kDeclarativeNetRequestAction,
            signal.GetType());
  const auto& dnr_action_signal =
      static_cast<const DeclarativeNetRequestActionSignal&>(signal);

  // Only REDIRECT action is supported currently. Ignore all other action types.
  if (dnr_action_signal.action_details().type() !=
      DeclarativeNetRequestActionInfo::REDIRECT) {
    return;
  }

  // Ignore the signal if both URLs (request and redirect) are empty (sanity
  // check).
  if (dnr_action_signal.action_details().request_url().empty() &&
      dnr_action_signal.action_details().redirect_url().empty()) {
    return;
  }

  // Retrieve the signal info store entry for this extension from the store. If
  // this is the first signal for an extension, a new entry is created in the
  // store.
  DeclarativeNetRequestActionInfoStoreEntry& info_store_entry =
      declarative_net_request_action_info_store_[dnr_action_signal
                                                     .extension_id()];
  ActionDetailsMap& action_details_map = info_store_entry.action_details_map;

  const std::string action_details_id =
      dnr_action_signal.GetUniqueActionDetailsId();
  auto action_details_it = action_details_map.find(action_details_id);
  if (action_details_it != action_details_map.end()) {
    // If an action detail with the same arguments has been invoked
    // before, simply increment the count for the corresponding record.
    auto count = action_details_it->second.count();
    action_details_it->second.set_count(count + 1);
  } else if (action_details_map.size() < max_unique_action_details_) {
    // For new action details, process the signal only if under max limit, i.e,
    // add the ActionDetails object to the call details map.
    action_details_map.emplace(action_details_id,
                               dnr_action_signal.action_details());
  }
}

std::unique_ptr<ExtensionTelemetryReportRequest_SignalInfo>
DeclarativeNetRequestActionSignalProcessor::GetSignalInfoForReport(
    const extensions::ExtensionId& extension_id) {
  auto declarative_net_request_action_info_store_entry =
      declarative_net_request_action_info_store_.find(extension_id);
  if (declarative_net_request_action_info_store_entry ==
      declarative_net_request_action_info_store_.end()) {
    return nullptr;
  }

  // Create the signal info protobuf.
  auto signal_info =
      std::make_unique<ExtensionTelemetryReportRequest_SignalInfo>();
  DeclarativeNetRequestActionInfo* dnr_action_info =
      signal_info->mutable_declarative_net_request_action_info();

  for (auto& action_detail : declarative_net_request_action_info_store_entry
                                 ->second.action_details_map) {
    *dnr_action_info->add_action_details() = std::move(action_detail.second);
  }

  // Finally, clear the data in the info store.
  declarative_net_request_action_info_store_.erase(
      declarative_net_request_action_info_store_entry);

  return signal_info;
}

bool DeclarativeNetRequestActionSignalProcessor::HasDataToReportForTest()
    const {
  return !declarative_net_request_action_info_store_.empty();
}

void DeclarativeNetRequestActionSignalProcessor::
    SetMaxUniqueActionDetailsForTest(size_t max_unique_action_details) {
  max_unique_action_details_ = max_unique_action_details;
}

}  // namespace safe_browsing
