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

using GetArgsInfo =
    ExtensionTelemetryReportRequest::SignalInfo::CookiesGetInfo::GetArgsInfo;

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
  GetArgsInfos& get_args_infos = store_entry.get_args_infos;

  const std::string arg_set_id = cg_signal.getUniqueArgSetId();
  auto get_args_infos_it = get_args_infos.find(arg_set_id);
  if (get_args_infos_it != get_args_infos.end()) {
    // If a cookies.get() API with the same arguments has been invoked
    // before, simply increment the count for the corresponding record.
    auto count = get_args_infos_it->second.count();
    get_args_infos_it->second.set_count(count + 1);

  } else if (get_args_infos.size() < max_arg_sets_) {
    // For new argument sets, process only if under max limit.
    // Create new GetArgsInfo object with its unique args set id key.
    GetArgsInfo get_args_info;
    get_args_info.set_name(cg_signal.name());
    get_args_info.set_store_id(cg_signal.store_id());
    get_args_info.set_url(cg_signal.url());
    get_args_info.set_count(1);

    get_args_infos.emplace(arg_set_id, get_args_info);

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
  auto signal_info =
      std::make_unique<ExtensionTelemetryReportRequest_SignalInfo>();
  ExtensionTelemetryReportRequest_SignalInfo_CookiesGetInfo* cookies_get_info =
      signal_info->mutable_cookies_get_info();

  for (auto& get_args_infos_it : cookies_get_store_it->second.get_args_infos) {
    *cookies_get_info->add_get_args_info() =
        std::move(get_args_infos_it.second);
  }
  cookies_get_info->set_max_exceeded_args_count(
      cookies_get_store_it->second.max_exceeded_arg_sets_count);

  // Finally, clear the data in the argument sets store.
  cookies_get_store_.erase(cookies_get_store_it);

  return signal_info;
}

bool CookiesGetSignalProcessor::HasDataToReportForTest() const {
  return !cookies_get_store_.empty();
}

void CookiesGetSignalProcessor::SetMaxArgSetsForTest(size_t max_arg_sets) {
  max_arg_sets_ = max_arg_sets;
}

}  // namespace safe_browsing
