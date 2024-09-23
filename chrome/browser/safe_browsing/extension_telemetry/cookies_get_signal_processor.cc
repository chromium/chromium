// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/extension_telemetry/cookies_get_signal_processor.h"

#include "base/check_op.h"
#include "chrome/browser/safe_browsing/extension_telemetry/cookies_get_signal.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"

namespace safe_browsing {

namespace {

// Used to limit the number of unique argument sets stored for each extension.
constexpr size_t kMaxArgSets = 100;

}  // namespace

CookiesGetSignalProcessor::CallData::CallData() = default;
CookiesGetSignalProcessor::CallData::~CallData() = default;
CookiesGetSignalProcessor::CallData::CallData(const CallData&) = default;

CookiesGetSignalProcessor::CookiesGetStoreEntry::CookiesGetStoreEntry() =
    default;
CookiesGetSignalProcessor::CookiesGetStoreEntry::~CookiesGetStoreEntry() =
    default;
CookiesGetSignalProcessor::CookiesGetStoreEntry::CookiesGetStoreEntry(
    const CookiesGetStoreEntry& src) = default;
CookiesGetSignalProcessor::CookiesGetSignalProcessor()
    : max_arg_sets_(kMaxArgSets) {}
CookiesGetSignalProcessor::~CookiesGetSignalProcessor() = default;

void CookiesGetSignalProcessor::ProcessSignal(const ExtensionSignal& signal) {
  // Validate GetArgs signal.
  DCHECK_EQ(ExtensionSignalType::kCookiesGet, signal.GetType());
  const auto& cg_signal = static_cast<const CookiesGetSignal&>(signal);

  // Retrieve store entry for extension. If this is the first signal for an
  // extension, a new entry is created in the store.
  CookiesGetStoreEntry& store_entry =
      cookies_get_store_[cg_signal.extension_id()];
  CallDataMap& call_data_map = store_entry.call_data_map;

  std::string arg_set_id = cg_signal.getUniqueArgSetId();
  auto call_data_it = call_data_map.find(arg_set_id);
  if (call_data_it != call_data_map.end()) {
    // If a cookies.get() API with the same arguments has been invoked
    // before, simply increment the count for the corresponding record
    // and save the associated JS callstack data.
    auto& [args_info, js_callstacks] = call_data_it->second;
    auto count = args_info.count();
    args_info.set_count(count + 1);
    js_callstacks.Add(cg_signal.js_callstack());
  } else if (call_data_map.size() < max_arg_sets_) {
    // For new argument sets, process only if under max limit.
    // Create a new entry for the call data map.
    CallData call_data;
    call_data.args_info.set_name(cg_signal.name());
    call_data.args_info.set_store_id(cg_signal.store_id());
    call_data.args_info.set_url(cg_signal.url());
    call_data.args_info.set_count(1);
    // Save the associated JS callstack data as well.
    call_data.js_callstacks.Add(cg_signal.js_callstack());

    call_data_map.emplace(std::move(arg_set_id), std::move(call_data));
  } else {
    // Otherwise, increment max exceeded argument sets count.
    store_entry.max_exceeded_arg_sets_count++;
  }
}

std::unique_ptr<ExtensionTelemetryReportRequest_SignalInfo>
CookiesGetSignalProcessor::GetSignalInfoForReport(
    const extensions::ExtensionId& extension_id) {
  auto cookies_get_store_it = cookies_get_store_.find(extension_id);
  if (cookies_get_store_it == cookies_get_store_.end())
    return nullptr;

  // Create the signal info protobuf.
  auto signal_info_pb =
      std::make_unique<ExtensionTelemetryReportRequest_SignalInfo>();
  ExtensionTelemetryReportRequest_SignalInfo_CookiesGetInfo*
      cookies_get_info_pb = signal_info_pb->mutable_cookies_get_info();

  for (auto& [key, call_data] : cookies_get_store_it->second.call_data_map) {
    // Get the JS callstacks associated with this arg set and add them to the
    // GetArgsInfo message.
    auto js_callstacks = call_data.js_callstacks.GetAll();
    call_data.args_info.mutable_js_callstacks()->Assign(js_callstacks.begin(),
                                                        js_callstacks.end());
    *cookies_get_info_pb->add_get_args_info() = std::move(call_data.args_info);
  }
  cookies_get_info_pb->set_max_exceeded_args_count(
      cookies_get_store_it->second.max_exceeded_arg_sets_count);

  // Finally, clear the data in the argument sets store.
  cookies_get_store_.erase(cookies_get_store_it);

  return signal_info_pb;
}

bool CookiesGetSignalProcessor::HasDataToReportForTest() const {
  return !cookies_get_store_.empty();
}

void CookiesGetSignalProcessor::SetMaxArgSetsForTest(size_t max_arg_sets) {
  max_arg_sets_ = max_arg_sets;
}

}  // namespace safe_browsing
