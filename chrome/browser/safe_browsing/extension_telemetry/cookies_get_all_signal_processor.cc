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

using GetAllArgsInfo = ExtensionTelemetryReportRequest::SignalInfo::
    CookiesGetAllInfo::GetAllArgsInfo;

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
  GetAllArgsInfos& get_all_args_infos = store_entry.get_all_args_infos;

  const std::string arg_set_id = cga_signal.getUniqueArgSetId();
  auto get_all_args_infos_it = get_all_args_infos.find(arg_set_id);
  if (get_all_args_infos_it != get_all_args_infos.end()) {
    // If a cookies.GetAll() API with the same arguments has been invoked
    // before, simply increment the count for the corresponding record.
    auto count = get_all_args_infos_it->second.count();
    get_all_args_infos_it->second.set_count(count + 1);

  } else if (get_all_args_infos.size() < max_arg_sets_) {
    // For new argument sets, process only if under max limit.
    // Create new GetAllArgsInfo object with its unique args set id key.
    GetAllArgsInfo get_all_args_info;
    get_all_args_info.set_domain(cga_signal.domain());
    get_all_args_info.set_name(cga_signal.name());
    get_all_args_info.set_path(cga_signal.path());
    get_all_args_info.set_secure(cga_signal.secure());
    get_all_args_info.set_store_id(cga_signal.store_id());
    get_all_args_info.set_url(cga_signal.url());
    get_all_args_info.set_count(1);
    get_all_args_info.set_is_session(cga_signal.is_session());

    get_all_args_infos.emplace(arg_set_id, get_all_args_info);

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
  auto signal_info =
      std::make_unique<ExtensionTelemetryReportRequest_SignalInfo>();
  ExtensionTelemetryReportRequest_SignalInfo_CookiesGetAllInfo*
      cookies_get_all_info = signal_info->mutable_cookies_get_all_info();

  for (auto& get_all_args_infos_it :
       cookies_get_all_store_it->second.get_all_args_infos) {
    *cookies_get_all_info->add_get_all_args_info() =
        std::move(get_all_args_infos_it.second);
  }
  cookies_get_all_info->set_max_exceeded_args_count(
      cookies_get_all_store_it->second.max_exceeded_arg_sets_count);

  // Finally, clear the data in the argument sets store.
  cookies_get_all_store_.erase(cookies_get_all_store_it);

  return signal_info;
}

bool CookiesGetAllSignalProcessor::HasDataToReportForTest() const {
  return !cookies_get_all_store_.empty();
}

void CookiesGetAllSignalProcessor::SetMaxArgSetsForTest(size_t max_arg_sets) {
  max_arg_sets_ = max_arg_sets;
}

}  // namespace safe_browsing
