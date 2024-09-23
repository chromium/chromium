// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/extension_telemetry/potential_password_theft_signal_processor.h"

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_service.h"
#include "chrome/browser/safe_browsing/extension_telemetry/password_reuse_signal.h"
#include "chrome/browser/safe_browsing/extension_telemetry/remote_host_contacted_signal.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "crypto/sha2.h"

namespace safe_browsing {

// The time proximity between a password reuse event and a remote host contact
// event. Remote contacted hosts that take longer than it will not be considered
// relevant.
const base::TimeDelta kPasswordTheftLatency = base::Seconds(1);

PotentialPasswordTheftSignalProcessor::PotentialPasswordTheftSignalProcessor() =
    default;
PotentialPasswordTheftSignalProcessor::
    ~PotentialPasswordTheftSignalProcessor() = default;

void PotentialPasswordTheftSignalProcessor::ProcessSignal(
    const ExtensionSignal& signal) {
  DCHECK(signal.GetType() == ExtensionSignalType::kRemoteHostContacted ||
         signal.GetType() == ExtensionSignalType::kPasswordReuse);
  if (!base::FeatureList::IsEnabled(
          safe_browsing::kExtensionTelemetryPotentialPasswordTheft)) {
    return;
  }
  base::Time signal_creation_time = base::Time::NowFromSystemTime();
  extensions::ExtensionId extension_id;
  // Process remote host contacted signal.
  if (signal.GetType() == ExtensionSignalType::kRemoteHostContacted) {
    const auto& rhc_signal =
        static_cast<const RemoteHostContactedSignal&>(signal);
    // Extract only the host portion of the remote host URLs.
    std::string host_url = rhc_signal.remote_host_url().host();
    extension_id = rhc_signal.extension_id();
    (remote_host_url_queue_[extension_id])
        .emplace_back(std::make_pair(host_url, signal_creation_time));
  } else {
    // Process password reuse signal.
    const auto& pw_reuse_signal =
        static_cast<const PasswordReuseSignal&>(signal);
    extension_id = pw_reuse_signal.extension_id();
    PasswordReuseInfo password_reuse_info =
        pw_reuse_signal.password_reuse_info();
    (password_queue_[extension_id])
        .emplace(std::make_pair(password_reuse_info, signal_creation_time));
  }
  UpdateDataStores(extension_id, signal_creation_time);
}

/**
 * This method contains the main logic for signal processing.
 * Use two queues to store password reuse events and remote host contacted
 * events. Cycle through the password reuse queue to find individual password
 * reuse events, then look up the remote host events that happened within 1s in
 * the other queue. Generate the combined potential password theft signal if we
 * find such remote host events.
 */
void PotentialPasswordTheftSignalProcessor::UpdateDataStores(
    extensions::ExtensionId extension_id,
    base::Time current_time) {
  auto remote_host_url_queue_it = remote_host_url_queue_.find(extension_id);
  auto password_queue_it = password_queue_.find(extension_id);
  // We are interested only in password re-use and remote host contact events
  // that occur in close proximity in time. Do not proceed unless we have events
  // in both queues.
  if (password_queue_it == password_queue_.end() ||
      remote_host_url_queue_it == remote_host_url_queue_.end()) {
    return;
  }

  auto& password_queue = password_queue_it->second;
  auto& remote_host_url_queue = remote_host_url_queue_it->second;
  while (!password_queue.empty()) {
    // Exit if the password at the front is not old enough to report;
    // Recorded less than kPasswordTheftLatency ago.
    if (password_queue.front().second + kPasswordTheftLatency > current_time) {
      return;
    }
    // Find the start time and end time based on password timestamps.
    base::Time start_recording_time = password_queue.front().second;
    base::Time end_recording_time =
        start_recording_time + kPasswordTheftLatency;
    PasswordReuseInfo& curr_pw_reuse_info = password_queue.front().first;
    std::vector<std::string>& matching_domains =
        curr_pw_reuse_info.matching_domains;
    uint64_t reused_password_hash = curr_pw_reuse_info.reused_password_hash;

    // Temporary containers to stage domain/url data.
    std::vector<std::string> temp_remote_host_url_queue;

    // Remove the remote host domains that were contacted before the first
    // password reuse event recorded, aka the start timestamp.
    while (!remote_host_url_queue.empty() &&
           remote_host_url_queue.front().second < start_recording_time) {
      remote_host_url_queue.pop_front();
    }

    // Cycle through the remote host url queue and add all the events that fall
    // between the start and end times.
    for (auto& remote_host_url_pair : remote_host_url_queue) {
      if (remote_host_url_pair.second > end_recording_time) {
        break;
      }
      std::string& host_url = remote_host_url_pair.first;
      // Each password reuse event has a reputable domain list, aka
      // matching_domains. Remote host urls that are in the reputable domain
      // lists associated with the password reuse events will not be added.
      if (!base::Contains(matching_domains, host_url)) {
        temp_remote_host_url_queue.push_back(host_url);
      }
    }

    if (temp_remote_host_url_queue.empty()) {
      password_queue.pop();
      return;
    }
    // Push the remote host urls/domains into store that were recorded within
    // the valid time window calculated above.
    for (std::string& host_url : temp_remote_host_url_queue) {
      ++((remote_host_url_store_[extension_id])[host_url]);
    }
    // Update the reuse information for the password so that we always have the
    // latest information associated with the reused saved password.
    auto password_reuse_info_it =
        (password_store_[extension_id]).find(reused_password_hash);
    if (password_reuse_info_it == (password_store_[extension_id]).end()) {
      // If reused_password_hash does not exist in the store, store the current
      // password info to it.
      (password_store_[extension_id])[reused_password_hash] =
          curr_pw_reuse_info;
    } else {
      // If reused_password_hash already exists in the store, update the stored
      // password reuse info.
      UpdatePasswordReuseInfo(
          (password_store_[extension_id])[reused_password_hash],
          curr_pw_reuse_info);
    }
    password_queue.pop();
  }
}

void PotentialPasswordTheftSignalProcessor::UpdatePasswordReuseInfo(
    PasswordReuseInfo& reuse_info,
    PasswordReuseInfo& incoming_reuse_info) {
  std::vector<std::string>& matching_domains = reuse_info.matching_domains;
  reuse_info.reused_password_hash = incoming_reuse_info.reused_password_hash;
  reuse_info.matches_signin_password =
      incoming_reuse_info.matches_signin_password;
  reuse_info.reused_password_account_type =
      incoming_reuse_info.reused_password_account_type;
  reuse_info.count += 1;
  for (auto& domain : incoming_reuse_info.matching_domains) {
    // Update the matching domain list once we find new domains.
    if (!base::Contains(matching_domains, domain)) {
      matching_domains.push_back(domain);
    }
  }
}

std::unique_ptr<ExtensionTelemetryReportRequest_SignalInfo>
PotentialPasswordTheftSignalProcessor::GetSignalInfoForReport(
    const extensions::ExtensionId& extension_id) {
  auto password_store_it = password_store_.find(extension_id);
  auto remote_host_url_store_it = remote_host_url_store_.find(extension_id);
  if (password_store_it == password_store_.end() ||
      remote_host_url_store_it == remote_host_url_store_.end()) {
    return nullptr;
  }
  // Create the signal info protobuf.
  auto signal_info =
      std::make_unique<ExtensionTelemetryReportRequest_SignalInfo>();
  ExtensionTelemetryReportRequest_SignalInfo_PotentialPasswordTheftInfo*
      potential_password_theft_info =
          signal_info->mutable_potential_password_theft_info();
  for (auto& password_w_count_it : password_store_it->second) {
    PasswordReuseInfo& reuse_info = password_w_count_it.second;
    ExtensionTelemetryReportRequest_SignalInfo_PotentialPasswordTheftInfo_PasswordReuseInfo*
        password_pb =
            potential_password_theft_info->add_reused_password_infos();
    if (reuse_info.matches_signin_password) {
      password_pb->set_is_chrome_signin_password(true);
    }
    for (const auto& domain : reuse_info.matching_domains) {
      password_pb->add_domains_matching_password(domain);
    }
    password_pb->set_count(reuse_info.count);
    *password_pb->mutable_reused_password_account_type() =
        ConvertToExtensionTelemetryReusedPasswordAccountType(
            reuse_info.reused_password_account_type);
  }
  for (auto& remote_host_url_w_count_it : remote_host_url_store_it->second) {
    ExtensionTelemetryReportRequest_SignalInfo_PotentialPasswordTheftInfo_RemoteHostData*
        remote_host_url_pb =
            potential_password_theft_info->add_remote_hosts_data();
    remote_host_url_pb->set_remote_host_url(
        std::move(remote_host_url_w_count_it.first));
    remote_host_url_pb->set_count(remote_host_url_w_count_it.second);
  }

  // Record the combined signal, kPotentialPasswordTheft, which is derived from
  // kPasswordReuse and kRemoteHostContacted. The signal is created once every
  // report creation time if there is potential password theft data available.
  ExtensionTelemetryService::RecordSignalType(
      ExtensionSignalType::kPotentialPasswordTheft);

  // Clear the data in the stores.
  // Following two iters are guaranteed to exist because
  // password_hash_remote_host_url_pair_store_it is not null.
  remote_host_url_store_.erase(remote_host_url_store_it);
  password_store_.erase(password_store_it);

  // Return signal info for report.
  return signal_info;
}

ExtensionTelemetryReusedPasswordAccountType
PotentialPasswordTheftSignalProcessor::
    ConvertToExtensionTelemetryReusedPasswordAccountType(
        LoginReputationClientReusedPasswordAccountType
            login_rep_reused_pw_account_type) {
  ExtensionTelemetryReusedPasswordAccountType
      extension_telemetry_pw_account_type;
  extension_telemetry_pw_account_type.set_is_account_syncing(
      login_rep_reused_pw_account_type.is_account_syncing());
  switch (login_rep_reused_pw_account_type.account_type()) {
    case LoginReputationClientReusedPasswordAccountType::UNKNOWN: {
      extension_telemetry_pw_account_type.set_account_type(
          ExtensionTelemetryReusedPasswordAccountType::UNKNOWN);
      return extension_telemetry_pw_account_type;
    }
    case LoginReputationClientReusedPasswordAccountType::GSUITE: {
      extension_telemetry_pw_account_type.set_account_type(
          ExtensionTelemetryReusedPasswordAccountType::GSUITE);
      return extension_telemetry_pw_account_type;
    }
    case LoginReputationClientReusedPasswordAccountType::GMAIL: {
      extension_telemetry_pw_account_type.set_account_type(
          ExtensionTelemetryReusedPasswordAccountType::GMAIL);
      return extension_telemetry_pw_account_type;
    }
    case LoginReputationClientReusedPasswordAccountType::NON_GAIA_ENTERPRISE: {
      extension_telemetry_pw_account_type.set_account_type(
          ExtensionTelemetryReusedPasswordAccountType::NON_GAIA_ENTERPRISE);
      return extension_telemetry_pw_account_type;
    }
    case LoginReputationClientReusedPasswordAccountType::SAVED_PASSWORD: {
      extension_telemetry_pw_account_type.set_account_type(
          ExtensionTelemetryReusedPasswordAccountType::SAVED_PASSWORD);
      return extension_telemetry_pw_account_type;
    }
  }
  NOTREACHED_IN_MIGRATION();
  return extension_telemetry_pw_account_type;
}

bool PotentialPasswordTheftSignalProcessor::IsPasswordQueueEmptyForTest() {
  return password_queue_.empty();
}

bool PotentialPasswordTheftSignalProcessor::IsRemoteHostURLQueueEmptyForTest() {
  return remote_host_url_queue_.empty();
}

bool PotentialPasswordTheftSignalProcessor::HasDataToReportForTest() const {
  return !remote_host_url_store_.empty() && !password_store_.empty();
}

}  // namespace safe_browsing
