// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/saas_usage/saas_usage_report_uploader_desktop.h"

#include <optional>
#include <string>
#include <string_view>

#include "base/functional/callback.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client_factory.h"
#include "chrome/browser/enterprise/util/affiliation.h"
#include "chrome/browser/policy/policy_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/reporting_util.h"
#include "components/enterprise/browser/controller/browser_dm_token_storage.h"
#include "components/enterprise/connectors/core/realtime_reporting_client_base.h"
#include "components/policy/core/common/cloud/realtime_reporting_job_configuration.h"
#include "components/policy/core/common/policy_logger.h"

namespace enterprise_reporting {

namespace {

std::optional<std::string> GetBrowserDMToken() {
  policy::DMToken dm_token =
      policy::BrowserDMTokenStorage::Get()->RetrieveDMToken();
  if (dm_token.is_valid()) {
    return dm_token.value();
  }

  return std::nullopt;
}

std::optional<std::string> GetProfileDMToken(Profile* profile) {
  return reporting::GetUserDmToken(profile);
}

}  // namespace

// =============================================================================
// SaasUsageReportUploaderDesktop base class implementation.
// =============================================================================

SaasUsageReportUploaderDesktop::SaasUsageReportUploaderDesktop(
    std::string_view uploader_name)
    : uploader_name_(uploader_name) {}

void SaasUsageReportUploaderDesktop::UploadReport(
    const ::chrome::cros::reporting::proto::SaasUsageReportEvent& report,
    base::OnceCallback<void(bool)> upload_callback) {
  if (!base::FeatureList::IsEnabled(
          policy::kUploadRealtimeReportingEventsUsingProto)) {
    LOG_POLICY(INFO, REPORTING)
        << "Real time reporting proto feature is not enabled. Skipping "
           "SaaS usage report upload.";
    return;
  }

  auto* client = GetRealTimeReportingClient();
  if (!client) {
    LOG_POLICY(ERROR, REPORTING)
        << "No real time reporting client found for report upload. "
        << "Skipping SaaS usage report upload.";
    return;
  }

  auto dm_token = GetDMToken();
  if (!dm_token) {
    LOG_POLICY(WARNING, REPORTING) << "No DM token found for report upload. "
                                      "Skipping SaaS usage report upload.";
    return;
  }

  VLOG_POLICY(1, REPORTING)
      << "Sending " << uploader_name_ << " SaaS usage report with "
      << report.domain_metrics_size() << " domain metrics.";
  ::chrome::cros::reporting::proto::Event event;
  *event.mutable_saas_usage_report_event() = report;
  client->ReportSaasUsageEvent(event, ShouldUseProfileClient(),
                               dm_token.value(), std::move(upload_callback));
}

// =============================================================================
// SaasUsageProfileReportUploaderDesktop implementation.
// =============================================================================

SaasUsageProfileReportUploaderDesktop::SaasUsageProfileReportUploaderDesktop(
    Profile* profile)
    : SaasUsageReportUploaderDesktop("profile"),
      profile_(raw_ref<Profile>::from_ptr(profile)) {}

enterprise_connectors::RealtimeReportingClientBase*
SaasUsageProfileReportUploaderDesktop::GetRealTimeReportingClient() {
  return enterprise_connectors::RealtimeReportingClientFactory::GetForProfile(
      &profile_.get());
}

bool SaasUsageProfileReportUploaderDesktop::ShouldUseProfileClient() {
  // We use the browser client for affiliated profiles to match the behaviour
  // of the realtime reporting pipeline.
  // Server will use profile id from the report to distinguish between browser
  // and profile reports.
  return !enterprise_util::IsProfileAffiliated(&profile_.get());
}

std::optional<std::string> SaasUsageProfileReportUploaderDesktop::GetDMToken() {
  return ShouldUseProfileClient() ? GetProfileDMToken(&profile_.get())
                                  : GetBrowserDMToken();
}

// =============================================================================
// SaasUsageBrowserReportUploaderDesktop implementation.
// =============================================================================

SaasUsageBrowserReportUploaderDesktop::SaasUsageBrowserReportUploaderDesktop()
    : SaasUsageReportUploaderDesktop("browser") {}

bool SaasUsageBrowserReportUploaderDesktop::ShouldUseProfileClient() {
  return false;
}

std::optional<std::string> SaasUsageBrowserReportUploaderDesktop::GetDMToken() {
  return GetBrowserDMToken();
}

enterprise_connectors::RealtimeReportingClientBase*
SaasUsageBrowserReportUploaderDesktop::GetRealTimeReportingClient() {
  // Browser-level reporting can use client from any profile.
  // Report scheduler ensures that the client is available before scheduling
  // the report upload.
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
