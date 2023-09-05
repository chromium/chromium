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
  // Extract only the host portion of the urls.
  std::string host_url = rhc_signal.remote_host_url().host();
  ++(((remote_host_info_store_[rhc_signal.extension_id()])
          [rhc_signal.protocol_type()])[host_url]);
}

std::unique_ptr<ExtensionTelemetryReportRequest_SignalInfo>
RemoteHostContactedSignalProcessor::GetSignalInfoForReport(
    const extensions::ExtensionId& extension_id) {
  auto remote_host_info_store_it = remote_host_info_store_.find(extension_id);
  if (remote_host_info_store_it == remote_host_info_store_.end())
    return nullptr;

  // Create the signal info protobuf.
  auto signal_info =
      std::make_unique<ExtensionTelemetryReportRequest_SignalInfo>();

  ExtensionTelemetryReportRequest_SignalInfo_RemoteHostContactedInfo*
      remote_host_contacted_info =
          signal_info->mutable_remote_host_contacted_info();

  for (auto& remote_host_urls_by_protocol_it :
       remote_host_info_store_it->second) {
    for (auto& remote_host_urls_it : remote_host_urls_by_protocol_it.second) {
      ExtensionTelemetryReportRequest_SignalInfo_RemoteHostContactedInfo_RemoteHostInfo*
          remote_host_pb = remote_host_contacted_info->add_remote_host();
      remote_host_pb->set_url(std::move(remote_host_urls_it.first));
      remote_host_pb->set_contact_count(remote_host_urls_it.second);
      remote_host_pb->set_connection_protocol(
          remote_host_urls_by_protocol_it.first);
    }
  }

  if (base::FeatureList::IsEnabled(
          kExtensionTelemetryInterceptRemoteHostsContactedInRenderer)) {
    remote_host_contacted_info->set_collected_from_new_interception(true);
  }

  // Clear the data in the remote host urls store.
  remote_host_info_store_.erase(remote_host_info_store_it);

  return signal_info;
}

bool RemoteHostContactedSignalProcessor::HasDataToReportForTest() const {
  return !remote_host_info_store_.empty();
}

}  // namespace safe_browsing
