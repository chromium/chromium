// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/extension_telemetry/remote_host_contacted_signal_processor.h"

#include "base/check_op.h"
#include "chrome/browser/safe_browsing/extension_telemetry/remote_host_contacted_signal.h"
#include "components/safe_browsing/core/common/features.h"

namespace safe_browsing {

RemoteHostContactedSignalProcessor::RemoteHostContactedSignalProcessor() =
    default;
RemoteHostContactedSignalProcessor::~RemoteHostContactedSignalProcessor() =
    default;

void RemoteHostContactedSignalProcessor::ProcessSignal(
    const ExtensionSignal& signal) {
  DCHECK_EQ(ExtensionSignalType::kRemoteHostContacted, signal.GetType());
  const auto& rhc_signal =
      static_cast<const RemoteHostContactedSignal&>(signal);

  // Retrieve store entry for extension. If this is the first signal for an
  // extension, a new entry is created in the store.
  RemoteHostsContacted& remote_hosts_contacted =
      remote_host_contacted_store_[rhc_signal.extension_id()];

  const std::string rhc_id = rhc_signal.GetUniqueRemoteHostContactedId();
  auto rhc_signal_it = remote_hosts_contacted.find(rhc_id);
  if (rhc_signal_it != remote_hosts_contacted.end()) {
    // If the signal with the same fields has been invoked before, simply
    // increment the count for the corresponding record.
    auto count = rhc_signal_it->second.contact_count();
    rhc_signal_it->second.set_contact_count(count + 1);
  } else {
    // For new signals, create a new store entry with |rhc_id| as the key.
    RemoteHostInfo remote_host_info;
    remote_host_info.set_url(rhc_signal.remote_host_url().host());
    remote_host_info.set_connection_protocol(rhc_signal.protocol_type());
    remote_host_info.set_contacted_by(rhc_signal.contact_initiator());
    remote_host_info.set_contact_count(1);

    remote_hosts_contacted.emplace(rhc_id, remote_host_info);
  }
}

std::unique_ptr<ExtensionTelemetryReportRequest_SignalInfo>
RemoteHostContactedSignalProcessor::GetSignalInfoForReport(
    const extensions::ExtensionId& extension_id) {
  auto remote_host_info_store_entry =
      remote_host_contacted_store_.find(extension_id);
  if (remote_host_info_store_entry == remote_host_contacted_store_.end()) {
    return nullptr;
  }

  // Create the signal info protobuf.
  auto signal_info =
      std::make_unique<ExtensionTelemetryReportRequest_SignalInfo>();
  ExtensionTelemetryReportRequest_SignalInfo_RemoteHostContactedInfo*
      remote_host_contacted_info =
          signal_info->mutable_remote_host_contacted_info();

  for (auto& [id, remote_host_info] : remote_host_info_store_entry->second) {
    *remote_host_contacted_info->add_remote_host() =
        std::move(remote_host_info);
  }

  if (base::FeatureList::IsEnabled(
          kExtensionTelemetryInterceptRemoteHostsContactedInRenderer)) {
    remote_host_contacted_info->set_collected_from_new_interception(true);
  }

  // Clear the data in the remote host urls store.
  remote_host_contacted_store_.erase(remote_host_info_store_entry);

  return signal_info;
}

bool RemoteHostContactedSignalProcessor::HasDataToReportForTest() const {
  return !remote_host_contacted_store_.empty();
}

}  // namespace safe_browsing
