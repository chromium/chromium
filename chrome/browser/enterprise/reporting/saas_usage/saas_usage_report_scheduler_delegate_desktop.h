// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REPORTING_SAAS_USAGE_SAAS_USAGE_REPORT_SCHEDULER_DELEGATE_DESKTOP_H_
#define CHROME_BROWSER_ENTERPRISE_REPORTING_SAAS_USAGE_SAAS_USAGE_REPORT_SCHEDULER_DELEGATE_DESKTOP_H_

#include "base/functional/callback.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "components/enterprise/browser/reporting/saas_usage/saas_usage_report_scheduler.h"

class Profile;
class ProfileManager;

namespace enterprise_reporting {

// This class is responsible for observing profile-related events
// and notifying the SaasUsageReportScheduler when there is at least one
// profile with a RealtimeReportingClient.
// This delegate is used only by browser-level scheduler.
class SaasUsageReportSchedulerDelegateDesktop
    : public SaasUsageReportScheduler::Delegate,
      public ProfileManagerObserver,
      public ProfileObserver {
 public:
  SaasUsageReportSchedulerDelegateDesktop();
  SaasUsageReportSchedulerDelegateDesktop(
      const SaasUsageReportSchedulerDelegateDesktop&) = delete;
  SaasUsageReportSchedulerDelegateDesktop& operator=(
      const SaasUsageReportSchedulerDelegateDesktop&) = delete;
  ~SaasUsageReportSchedulerDelegateDesktop() override;

  // SaasUsageReportScheduler::Delegate:
  void SetReadyStateChangedCallback(base::RepeatingClosure callback) override;
  bool IsReady() override;

  // ProfileManagerObserver:
  void OnProfileAdded(Profile* profile) override;
  void OnProfileManagerDestroying() override;

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override;

 private:
  base::RepeatingClosure ready_state_changed_callback_;

  base::ScopedObservation<ProfileManager, ProfileManagerObserver>
      profile_manager_observation_{this};
  base::ScopedMultiSourceObservation<Profile, ProfileObserver>
      profile_observation_{this};
};

}  // namespace enterprise_reporting

#endif  // CHROME_BROWSER_ENTERPRISE_REPORTING_SAAS_USAGE_SAAS_USAGE_REPORT_SCHEDULER_DELEGATE_DESKTOP_H_
