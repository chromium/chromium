// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/extension_telemetry/cookies_get_all_signal_processor.h"

#include "base/check_op.h"
#include "chrome/browser/safe_browsing/extension_telemetry/cookies_get_all_signal.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"

namespace safe_browsing {

namespace {

// Used to limit the number of unique argument sets stored for each extension.
constexpr size_t kMaxArgSets = 100;

}  // namespace

CookiesGetAllSignalProcessor::CallData::CallData() = default;
CookiesGetAllSignalProcessor::CallData::~CallData() = default;
CookiesGetAllSignalProcessor::CallData::CallData(const CallData&) = default;

CookiesGetAllSignalProcessor::CookiesGetAllStoreEntry::
    CookiesGetAllStoreEntry() = default;
CookiesGetAllSignalProcessor::CookiesGetAllStoreEntry::
    ~CookiesGetAllStoreEntry() = default;
CookiesGetAllSignalProcessor::CookiesGetAllStoreEntry::CookiesGetAllStoreEntry(
    const CookiesGetAllStoreEntry& src) = default;
CookiesGetAllSignalProcessor::CookiesGetAllSignalProcessor()
    : max_arg_sets_(kMaxArgSets) {}
CookiesGetAllSignalProcessor::~CookiesGetAllSignalProcessor() = default;

void CookiesGetAllSignalProcessor::ProcessSignal(
    const ExtensionSignal& signal) {
  // Validate GetAllArgs signal.
  DCHECK_EQ(ExtensionSignalType::kCookiesGetAll, signal.GetType());
  const auto& cga_signal = static_cast<const CookiesGetAllSignal&>(signal);

  // Retrieve store entry for extension. If this is the first signal for an
  // extension, a new entry is created in the store.
  CookiesGetAllStoreEntry& store_entry =
      cookies_get_all_store_[cga_signal.extension_id()];
  CallDataMap& call_data_map = store_entry.call_data_map;

  std::string arg_set_id = cga_signal.getUniqueArgSetId();
  auto call_data_it = call_data_map.find(arg_set_id);
  if (call_data_it != call_data_map.end()) {
    // If a cookies.GetAll() API with the same arguments has been invoked
    // before, simply increment the count for the corresponding record and
    // save the associated JS callstack data.
    auto& [args_info, js_callstacks] = call_data_it->second;
    auto count = args_info.count();
    args_info.set_count(count + 1);
    js_callstacks.Add(cga_signal.js_callstack());
  } else if (call_data_map.size() < max_arg_sets_) {
    // For new argument sets, process only if under max limit.
    // Create a new entry for the call_data_map.
    CallData call_data;
    call_data.args_info.set_domain(cga_signal.domain());
    call_data.args_info.set_name(cga_signal.name());
    call_data.args_info.set_path(cga_signal.path());
    call_data.args_info.set_store_id(cga_signal.store_id());
    call_data.args_info.set_url(cga_signal.url());
    call_data.args_info.set_count(1);
    if (cga_signal.secure().has_value()) {
      call_data.args_info.set_secure(cga_signal.secure().value());
    }
    if (cga_signal.is_session().has_value()) {
      call_data.args_info.set_is_session(cga_signal.is_session().value());
    }
    // Save the associated JS callstack data as well.
    call_data.js_callstacks.Add(cga_signal.js_callstack());

    call_data_map.emplace(std::move(arg_set_id), std::move(call_data));
  } else {
    // Otherwise, increment max exceeded argument sets count.
    store_entry.max_exceeded_arg_sets_count++;
  }
}

std::unique_ptr<ExtensionTelemetryReportRequest_SignalInfo>
CookiesGetAllSignalProcessor::GetSignalInfoForReport(
    const extensions::ExtensionId& extension_id) {
  auto cookies_get_all_store_it = cookies_get_all_store_.find(extension_id);
  if (cookies_get_all_store_it == cookies_get_all_store_.end())
    return nullptr;

  // Create the signal info protobuf.
  auto signal_info_pb =
      std::make_unique<ExtensionTelemetryReportRequest_SignalInfo>();
  ExtensionTelemetryReportRequest_SignalInfo_CookiesGetAllInfo*
      cookies_get_all_info_pb = signal_info_pb->mutable_cookies_get_all_info();

  for (auto& [key, call_data] :
       cookies_get_all_store_it->second.call_data_map) {
    // Get the JS callstacks associated with this arg set and add them to the
    // GetAllArgsInfo message.
    auto js_callstacks = call_data.js_callstacks.GetAll();
    call_data.args_info.mutable_js_callstacks()->Assign(js_callstacks.begin(),
                                                        js_callstacks.end());
    *cookies_get_all_info_pb->add_get_all_args_info() =
        std::move(call_data.args_info);
  }
  cookies_get_all_info_pb->set_max_exceeded_args_count(
      cookies_get_all_store_it->second.max_exceeded_arg_sets_count);

  // Finally, clear the data in the argument sets store.
  cookies_get_all_store_.erase(cookies_get_all_store_it);

  return signal_info_pb;
}

bool CookiesGetAllSignalProcessor::HasDataToReportForTest() const {
  return !cookies_get_all_store_.empty();
}

void CookiesGetAllSignalProcessor::SetMaxArgSetsForTest(size_t max_arg_sets) {
  max_arg_sets_ = max_arg_sets;
}

}  // namespace safe_browsing
