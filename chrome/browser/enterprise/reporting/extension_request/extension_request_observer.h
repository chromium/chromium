// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REPORTING_EXTENSION_REQUEST_EXTENSION_REQUEST_OBSERVER_H_
#define CHROME_BROWSER_ENTERPRISE_REPORTING_EXTENSION_REQUEST_EXTENSION_REQUEST_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/enterprise/reporting/extension_request/extension_request_notification.h"
#include "chrome/browser/extensions/extension_management.h"

class Profile;

namespace enterprise_reporting {

// Observer for the extension policy to show the extension request notification
// if necessary.
class ExtensionRequestObserver
    : public extensions::ExtensionManagement::Observer {
 public:
  using ReportTrigger = base::RepeatingCallback<void(Profile*)>;
  explicit ExtensionRequestObserver(Profile* profile);
  ~ExtensionRequestObserver() override;
  ExtensionRequestObserver(const ExtensionRequestObserver&) = delete;
  ExtensionRequestObserver& operator=(const ExtensionRequestObserver&) = delete;

  bool IsReportEnabled();
  void EnableReport(ReportTrigger trigger);
  void DisableReport();

 private:
  // extensions::ExtensionManagement::Observer
  void OnExtensionManagementSettingsChanged() override;

  // Notifies when request pending list is updated.
  void OnPendingListChanged();

  // Shows notifications when requests are approved, rejected or
  // force-installed. It also closes the notification that is no longer needed.
  void ShowAllNotifications();

  void ShowNotification(ExtensionRequestNotification::NotifyType type);
  void CloseAllNotifications();

  void OnNotificationClosed(std::vector<std::string>&& extension_ids,
                            bool by_user);

  void RemoveExtensionsFromPendingList(
      const std::vector<std::string>& extension_ids);

  std::unique_ptr<ExtensionRequestNotification>
      notifications_[ExtensionRequestNotification::kNumberOfTypes];

  raw_ptr<Profile> profile_;

  PrefChangeRegistrar pref_change_registrar_;
  bool closing_notification_and_deleting_requests_ = false;
  ReportTrigger report_trigger_;

  base::WeakPtrFactory<ExtensionRequestObserver> weak_factory_{this};
};

}  // namespace enterprise_reporting

#endif  // CHROME_BROWSER_ENTERPRISE_REPORTING_EXTENSION_REQUEST_EXTENSION_REQUEST_OBSERVER_H_
