// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_CHILD_STATUS_REPORTING_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_CHILD_STATUS_REPORTING_SERVICE_H_

#include <memory>

#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"

class PrefChangeRegistrar;

namespace content {
class BrowserContext;
}

namespace policy {
class CloudPolicyClient;
class StatusUploader;
class UserCloudPolicyManagerChromeOS;
}  // namespace policy

namespace chromeos {

// Controls reporting for child user.
// Child user should be registered with DMServer and periodically upload the
// information about the device usage. The reports are only sent during user's
// session and they do not interfere with enterprise reporting that is
// controlled by DeviceCloudPolicyManagerChromeOS.
class ChildStatusReportingService : public KeyedService {
 public:
  explicit ChildStatusReportingService(content::BrowserContext* context);
  ChildStatusReportingService(const ChildStatusReportingService&) = delete;
  ChildStatusReportingService& operator=(const ChildStatusReportingService&) =
      delete;
  ~ChildStatusReportingService() override;

  // Returns true if the status report has been scheduled. Otherwise, returns
  // false.
  virtual bool RequestImmediateStatusReport();

  // Get the child's usage time so far today.
  base::TimeDelta GetChildScreenTime() const;

 private:
  // Creates new status uploader if parameters changed.
  void CreateStatusUploaderIfNeeded(policy::CloudPolicyClient* client);

  // Called when the UsageTimeLimits policy changes.
  void OnTimeLimitsPolicyChanged();

  // Helper object that controls device status collection/storage and uploads
  // gathered reports to the server.
  std::unique_ptr<policy::StatusUploader> status_uploader_;

  // Preference changes observer.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  // Day start/reset time used for aggregating activity data for child status
  // reporting.
  base::TimeDelta day_reset_time_;

  content::BrowserContext* const context_;

  policy::UserCloudPolicyManagerChromeOS* user_cloud_policy_manager_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_CHILD_STATUS_REPORTING_SERVICE_H_
