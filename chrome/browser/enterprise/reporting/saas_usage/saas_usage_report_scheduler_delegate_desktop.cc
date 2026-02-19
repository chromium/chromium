// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/saas_usage/saas_usage_report_scheduler_delegate_desktop.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/policy/core/common/policy_logger.h"

namespace enterprise_reporting {

SaasUsageReportSchedulerDelegateDesktop::
    SaasUsageReportSchedulerDelegateDesktop() {
  CHECK(g_browser_process->profile_manager());

  profile_manager_observation_.Observe(g_browser_process->profile_manager());
  for (Profile* loaded_profile :
       g_browser_process->profile_manager()->GetLoadedProfiles()) {
    OnProfileAdded(loaded_profile);
  }
}

SaasUsageReportSchedulerDelegateDesktop::
    ~SaasUsageReportSchedulerDelegateDesktop() = default;

void SaasUsageReportSchedulerDelegateDesktop::SetReadyStateChangedCallback(
    base::RepeatingClosure callback) {
  ready_state_changed_callback_ = callback;
}

bool SaasUsageReportSchedulerDelegateDesktop::IsReady() {
  return profile_observation_.IsObservingAnySource();
}

void SaasUsageReportSchedulerDelegateDesktop::OnProfileAdded(Profile* profile) {
  // If browser is managed, any profile with a RealtimeReportingClient will be
  // able to upload reports using browser DM token.
  if (!enterprise_connectors::RealtimeReportingClientFactory::GetForProfile(
          profile)) {
    return;
  }

  bool was_ready = IsReady();
  profile_observation_.AddObservation(profile);
  if (ready_state_changed_callback_ && !was_ready) {
    VLOG_POLICY(1, REPORTING)
        << "SaaS usage reporting is enabled because a reporting-enabled "
           "profile has been added.";
    ready_state_changed_callback_.Run();
  }
}

void SaasUsageReportSchedulerDelegateDesktop::OnProfileManagerDestroying() {
  profile_manager_observation_.Reset();
}

void SaasUsageReportSchedulerDelegateDesktop::OnProfileWillBeDestroyed(
    Profile* profile) {
  if (!profile_observation_.IsObservingSource(profile)) {
    return;
  }

  profile_observation_.RemoveObservation(profile);
  if (ready_state_changed_callback_ && !IsReady()) {
    VLOG_POLICY(1, REPORTING)
        << "SaaS usage reporting is disabled because the last "
           "reporting-enabled profile is being destroyed.";
    ready_state_changed_callback_.Run();
  }
}

}  // namespace enterprise_reporting
