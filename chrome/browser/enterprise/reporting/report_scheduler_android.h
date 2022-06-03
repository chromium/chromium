// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REPORTING_REPORT_SCHEDULER_ANDROID_H_
#define CHROME_BROWSER_ENTERPRISE_REPORTING_REPORT_SCHEDULER_ANDROID_H_

#include "components/enterprise/browser/reporting/report_scheduler.h"

namespace enterprise_reporting {

// Android implementation of the ReportScheduler delegate.
class ReportSchedulerAndroid : public ReportScheduler::Delegate {
 public:
  ReportSchedulerAndroid();
  ReportSchedulerAndroid(const ReportSchedulerAndroid&) = delete;
  ReportSchedulerAndroid& operator=(const ReportSchedulerAndroid&) = delete;

  ~ReportSchedulerAndroid() override;

  // ReportScheduler::Delegate implementation.
  PrefService* GetLocalState() override;
  void StartWatchingUpdatesIfNeeded(base::Time last_upload,
                                    base::TimeDelta upload_interval) override;
  void StopWatchingUpdates() override;
  void OnBrowserVersionUploaded() override;
  void StartWatchingExtensionRequestIfNeeded() override;
  void StopWatchingExtensionRequest() override;
  void OnExtensionRequestUploaded() override;
};

}  // namespace enterprise_reporting

#endif  // CHROME_BROWSER_ENTERPRISE_REPORTING_REPORT_SCHEDULER_ANDROID_H_
