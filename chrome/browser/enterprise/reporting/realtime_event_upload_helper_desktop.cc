// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/realtime_event_upload_helper_desktop.h"

#include <optional>
#include <string>
#include <string_view>

#include "base/check.h"
#include "base/feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client_factory.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/reporting_util.h"
#include "components/enterprise/browser/controller/browser_dm_token_storage.h"
#include "components/policy/core/common/cloud/realtime_reporting_job_configuration.h"
#include "components/policy/core/common/policy_logger.h"

namespace enterprise_reporting {

RealtimeEventUploadHelper::RealtimeEventUploadHelper(
    std::string_view reporting_scope,
    std::string_view event_name,
    Profile* profile)
    : reporting_scope_(reporting_scope),
      event_name_(event_name),
      profile_(profile) {}

RealtimeEventUploadHelper::~RealtimeEventUploadHelper() = default;

std::optional<RealtimeEventUploadHelper::ReportingContext>
RealtimeEventUploadHelper::PrepareUpload(bool per_profile) {
  // For new realtime reporting features, we only support the Protobuf pipeline.
  // Therefore, if kUploadRealtimeReportingEventsUsingProto is not enabled, we
  // must abort the upload entirely to prevent crashing the underlying
  // CloudPolicyClient, which strictly enforces this flag.
  if (!base::FeatureList::IsEnabled(
          policy::kUploadRealtimeReportingEventsUsingProto)) {
    VLOG_POLICY(1, REPORTING)
        << "Real time reporting proto feature is not enabled. Skipping "
        << reporting_scope_ << " " << event_name_ << " report upload.";
    return std::nullopt;
  }

  enterprise_connectors::RealtimeReportingClientBase*
      real_time_reporting_client = GetRealTimeReportingClient();

  if (!real_time_reporting_client) {
    LOG_POLICY(ERROR, REPORTING)
        << "No real time reporting client found for " << reporting_scope_ << " "
        << event_name_ << " report upload. Skipping upload.";
    return std::nullopt;
  }

  std::optional<std::string> dm_token = GetDMToken(per_profile);

  if (!dm_token) {
    LOG_POLICY(ERROR, REPORTING)
        << "No DM token found for " << reporting_scope_ << " " << event_name_
        << " report upload. Skipping upload.";
    return std::nullopt;
  }

  return ReportingContext{raw_ref(*real_time_reporting_client),
                          std::move(dm_token.value()), per_profile};
}

bool RealtimeEventUploadHelper::IsRealTimeReportingClientAvailable() const {
  return GetRealTimeReportingClient() != nullptr;
}

bool RealtimeEventUploadHelper::IsEligibleForUpload(bool per_profile) const {
  return base::FeatureList::IsEnabled(
             policy::kUploadRealtimeReportingEventsUsingProto) &&
         GetDMToken(per_profile).has_value();
}
std::optional<std::string> RealtimeEventUploadHelper::GetDMToken(
    bool per_profile) const {
#if BUILDFLAG(IS_CHROMEOS)
  // On ChromeOS, we must always use the profile DM token because
  // BrowserDMTokenStorage does not exist, and policy::GetDMToken(nullptr)
  // unconditionally returns an empty token.
  return reporting::GetUserDmToken(profile_);
#else
  if (per_profile) {
    return reporting::GetUserDmToken(profile_);
  }

  policy::DMToken dm_token =
      policy::BrowserDMTokenStorage::Get()->RetrieveDMToken();
  if (dm_token.is_valid()) {
    return dm_token.value();
  }

  return std::nullopt;
#endif
}

enterprise_connectors::RealtimeReportingClientBase*
RealtimeEventUploadHelper::GetRealTimeReportingClient() const {
  if (profile_) {
    return enterprise_connectors::RealtimeReportingClientFactory::GetForProfile(
        profile_);
  }

  if (!g_browser_process) {
    return nullptr;
  }

  if (!g_browser_process->profile_manager()) {
    return nullptr;
  }

  // TODO(crbug.com/527894269): RealtimeReportingClient is currently a
  // KeyedService and we retrieve a client from an arbitrary profile for
  // browser-level reporting. This is not ideal. We should refactor it to have a
  // browser-wide instance.
  for (auto* profile :
       g_browser_process->profile_manager()->GetLoadedProfiles()) {
    auto* client =
        enterprise_connectors::RealtimeReportingClientFactory::GetForProfile(
            profile);
    if (client) {
      return client;
    }
  }
  return nullptr;
}

}  // namespace enterprise_reporting
