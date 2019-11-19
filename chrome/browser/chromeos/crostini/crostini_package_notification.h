// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_PACKAGE_NOTIFICATION_H_
#define CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_PACKAGE_NOTIFICATION_H_

#include <memory>
#include <ostream>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_package_operation_status.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

namespace message_center {
class Notification;
}

namespace crostini {

class CrostiniPackageService;

// Notification for various Crostini package operations, such as installing
// from a package or uninstalling an existing app.
class CrostiniPackageNotification : public message_center::NotificationObserver,
                                    public CrostiniRegistryService::Observer {
 public:
  enum class NotificationType { PACKAGE_INSTALL, APPLICATION_UNINSTALL };

  // |app_name| should be empty for PACKAGE_INSTALL, non-empty for
  // APPLICATION_UNINSTALL.
  CrostiniPackageNotification(Profile* profile,
                              NotificationType notification_type,
                              PackageOperationStatus status,
                              const ContainerId& container_id,
                              const base::string16& app_name,
                              const std::string& notification_id,
                              CrostiniPackageService* installer_service);
  ~CrostiniPackageNotification() override;

  void UpdateProgress(PackageOperationStatus status, int progress_percent);

  void ForceAllowAutoHide();

  PackageOperationStatus GetOperationStatus() const;

  // message_center::NotificationObserver:
  void Close(bool by_user) override;

  void Click(const base::Optional<int>& button_index,
             const base::Optional<base::string16>& reply) override;

  // CrostiniRegistryService::Observer:
  void OnRegistryUpdated(
      CrostiniRegistryService* registry_service,
      const std::vector<std::string>& updated_apps,
      const std::vector<std::string>& removed_apps,
      const std::vector<std::string>& inserted_apps) override;

  int GetButtonCountForTesting();

 private:
  // A type giving the string, etc displayed for each notification type. Note
  // that we have the complete strings here, not just the string IDs, because
  // the call needed to generate the strings is slightly different between
  // notification types (specifically, uninstall notification strings usually
  // need an app_name, while installs do not).
  struct NotificationSettings {
    NotificationSettings();
    NotificationSettings(const NotificationSettings& rhs);
    ~NotificationSettings();
    base::string16 source;
    base::string16 queued_title;
    base::string16 queued_body;
    base::string16 progress_title;
    base::string16 progress_body;
    base::string16 success_title;
    base::string16 success_body;
    base::string16 failure_title;
    base::string16 failure_body;
  };

  void UpdateDisplayedNotification();

  static NotificationSettings GetNotificationSettingsForTypeAndAppName(
      NotificationType notification_type,
      const base::string16& app_name);

  const NotificationType notification_type_;
  PackageOperationStatus current_status_;

  // The most-recent time we entered the "RUNNING" state. Used for
  // guesstimating when we'll be done.
  base::TimeTicks running_start_time_;

  // These notifications are owned by the package service.
  CrostiniPackageService* package_service_;
  Profile* profile_;
  const NotificationSettings notification_settings_;

  std::unique_ptr<message_center::Notification> notification_;

  // True if we think the notification is visible.
  bool visible_;

  // If we show a launch button on completion, this is the app that will be
  // launched.
  std::string app_id_;

  ContainerId container_id_;

  std::set<std::string> inserted_apps_;
  int app_count_ = 0;

  base::WeakPtrFactory<CrostiniPackageNotification> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CrostiniPackageNotification);
};

}  // namespace crostini

#endif  // CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_PACKAGE_NOTIFICATION_H_
