// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REPORTING_REPORT_SCHEDULER_DESKTOP_H_
#define CHROME_BROWSER_ENTERPRISE_REPORTING_REPORT_SCHEDULER_DESKTOP_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/enterprise/reporting/extension_request/extension_request_observer_factory.h"
#include "chrome/browser/upgrade_detector/build_state_observer.h"
#include "components/enterprise/browser/reporting/report_scheduler.h"
#include "components/enterprise/browser/reporting/user_security_signals_service.h"

class Profile;

namespace enterprise_reporting {

// Desktop implementation of the ReportScheduler delegate.
class ReportSchedulerDesktop : public ReportScheduler::Delegate,
                               public UserSecuritySignalsService::Delegate,
                               public BuildStateObserver {
 public:
  ReportSchedulerDesktop();
  /* `profile` is used for profile reporting */
  explicit ReportSchedulerDesktop(Profile* profile);
  ReportSchedulerDesktop(const ReportSchedulerDesktop&) = delete;
  ReportSchedulerDesktop& operator=(const ReportSchedulerDesktop&) = delete;

  ~ReportSchedulerDesktop() override;

  // ReportScheduler::Delegate implementation.
  PrefService* GetPrefService() override;
  void OnInitializationCompleted() override;
  void StartWatchingUpdatesIfNeeded(base::Time last_upload,
                                    base::TimeDelta upload_interval) override;
  void StopWatchingUpdates() override;
  void OnBrowserVersionUploaded() override;

  policy::DMToken GetProfileDMToken() override;
  std::string GetProfileClientId() override;

  bool AreSecurityReportsEnabled() override;
  bool UseCookiesInUploads() override;
  void OnSecuritySignalsUploaded() override;

  // UserSecuritySignalsService::Delegate implementation.
  void OnReportEventTriggered(SecurityReportTrigger trigger) override;
  network::mojom::CookieManager* GetCookieManager() override;

  // BuildStateObserver implementation.
  void OnUpdate(const BuildState* build_state) override;

 private:
  raw_ptr<Profile> profile_;
  raw_ptr<PrefService> prefs_;

  // Only set for Profile-level schedulers.
  std::unique_ptr<UserSecuritySignalsService> user_security_signals_service_;
};

}  // namespace enterprise_reporting

#endif  // CHROME_BROWSER_ENTERPRISE_REPORTING_REPORT_SCHEDULER_DESKTOP_H_
