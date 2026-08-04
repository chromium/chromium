// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/browser_launch/browser_launch_event_uploader_desktop.h"

#include <optional>
#include <string_view>

#include "base/functional/callback.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/enterprise/connectors/core/realtime_reporting_client_base.h"
#include "components/policy/core/common/cloud/realtime_reporting_job_configuration.h"
#include "components/policy/core/common/policy_logger.h"

namespace enterprise_reporting {

BrowserLaunchEventUploaderDesktop::BrowserLaunchEventUploaderDesktop()
    : helper_("browser", "browser launch", nullptr), profile_(nullptr) {}

BrowserLaunchEventUploaderDesktop::BrowserLaunchEventUploaderDesktop(
    Profile* profile)
    : helper_("profile", "browser launch", profile), profile_(profile) {
  CHECK(profile);
}

BrowserLaunchEventUploaderDesktop::~BrowserLaunchEventUploaderDesktop() {
  if (upload_callback_) {
    std::move(upload_callback_)
        .Run(policy::CloudPolicyClient::Result(
            policy::CloudPolicyClient::NotRegistered()));
  }
}

std::string_view BrowserLaunchEventUploaderDesktop::GetMetricSuffix() const {
  return IsProfileReporting() ? "Profile" : "Browser";
}

void BrowserLaunchEventUploaderDesktop::UploadEvent(
    const ::chrome::cros::reporting::proto::BrowserLaunchEvent& event,
    base::OnceCallback<void(policy::CloudPolicyClient::Result)>
        upload_callback) {
  // Defer browser-level launch event uploads until a regular profile is loaded.
  // 1) Users can choose to launch Chrome with the Profile Picker (or launch
  //    with multiple profiles), meaning no regular profile is opened
  //    immediately after launch—only the System Profile is loaded.
  // 2) Real-time reporting currently relies on profile-keyed reporting clients
  //    (which exclude System Profiles), so no reporting client is available at
  //    browser startup to report browser-level events.
  // 3) Long-term plan: Support browser-level real-time reporting independent of
  //    user profiles (see b/492328505).
  // 4) Once browser-level reporting is available, the deferral and
  //    ProfileManagerObserver logic here should be reverted.
  auto context = helper_.PrepareUpload(IsProfileReporting());
  if (context) {
    DoUpload(*context, event, std::move(upload_callback));
  } else if (ShouldDeferUpload()) {
    DeferUpload(event, std::move(upload_callback));
  } else {
    std::move(upload_callback)
        .Run(policy::CloudPolicyClient::Result(
            policy::CloudPolicyClient::NotRegistered()));
  }
}

bool BrowserLaunchEventUploaderDesktop::ShouldDeferUpload() const {
  return !IsProfileReporting() &&
         !helper_.IsRealTimeReportingClientAvailable() &&
         helper_.IsEligibleForUpload(/*per_profile=*/false) &&
         g_browser_process && g_browser_process->profile_manager();
}

void BrowserLaunchEventUploaderDesktop::DeferUpload(
    const ::chrome::cros::reporting::proto::BrowserLaunchEvent& event,
    base::OnceCallback<void(policy::CloudPolicyClient::Result)>
        upload_callback) {
  VLOG_POLICY(1, REPORTING)
      << "No real time reporting client found for browser launch "
         "report upload yet. Waiting for a profile to be added.";
  event_ = event;
  upload_callback_ = std::move(upload_callback);
  profile_manager_observation_.Observe(g_browser_process->profile_manager());
}

void BrowserLaunchEventUploaderDesktop::OnProfileAdded(Profile* profile) {
  if (!enterprise_connectors::RealtimeReportingClientFactory::GetForProfile(
          profile)) {
    return;
  }

  auto context = helper_.PrepareUpload(/*per_profile=*/false);
  if (!context) {
    // Keep observing until an eligible profile is loaded or browser shutdown
    // occurs.
    return;
  }

  profile_manager_observation_.Reset();
  DoUpload(*context, event_, std::move(upload_callback_));
}

void BrowserLaunchEventUploaderDesktop::OnProfileManagerDestroying() {
  profile_manager_observation_.Reset();
  if (upload_callback_) {
    std::move(upload_callback_)
        .Run(policy::CloudPolicyClient::Result(
            policy::CloudPolicyClient::NotRegistered()));
  }
}

void BrowserLaunchEventUploaderDesktop::DoUpload(
    const RealtimeEventUploadHelper::ReportingContext& context,
    const ::chrome::cros::reporting::proto::BrowserLaunchEvent& event,
    base::OnceCallback<void(policy::CloudPolicyClient::Result)>
        upload_callback) {
  VLOG_POLICY(1, REPORTING) << "Sending " << helper_.reporting_scope() << " "
                            << helper_.event_name() << " report.";

  ::chrome::cros::reporting::proto::Event wrapper;
  *wrapper.mutable_browser_launch_event() = event;

  context.client->ReportBrowserLaunchEvent(wrapper, context.per_profile,
                                           context.dm_token,
                                           std::move(upload_callback));
}

}  // namespace enterprise_reporting
