// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVICE_NOTIFICATIONS_DEVICE_PINNED_NOTIFICATION_RENDERER_H_
#define CHROME_BROWSER_DEVICE_NOTIFICATIONS_DEVICE_PINNED_NOTIFICATION_RENDERER_H_

#include "ash/constants/notifier_catalogs.h"
#include "chrome/browser/device_notifications/device_system_tray_icon.h"
#include "chrome/browser/device_notifications/device_system_tray_icon_renderer.h"
#include "device_connection_tracker.h"
#include "ui/message_center/public/cpp/notification.h"

class DevicePinnedNotificationRenderer : public DeviceSystemTrayIconRenderer {
 public:
  explicit DevicePinnedNotificationRenderer(
      DeviceSystemTrayIcon* device_system_tray_icon,
      const std::string& notification_id_prefix,
#if BUILDFLAG(IS_CHROMEOS_ASH)
      const ash::NotificationCatalogName notification_catalog_name,
#endif
      const int message_id);
  DevicePinnedNotificationRenderer(const DevicePinnedNotificationRenderer&) =
      delete;
  DevicePinnedNotificationRenderer& operator=(
      const DevicePinnedNotificationRenderer&) = delete;
  ~DevicePinnedNotificationRenderer() override;

  // DeviceSystemTrayIconRenderer
  void AddProfile(Profile* profile) override;
  void RemoveProfile(Profile* profile) override;
  void NotifyConnectionUpdated(Profile* profile) override;

  // The notification id of the pinned notification.
  std::string GetNotificationId(Profile* profile);

 private:
  // Create a pinned notification for |profile| to indicate at least one
  // device is being accessed.
  std::unique_ptr<message_center::Notification> CreateNotification(
      Profile* profile);

  // Display |notification| in the system notification.
  void DisplayNotification(
      std::unique_ptr<message_center::Notification> notification);

  std::string notification_id_prefix_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::NotificationCatalogName notification_catalog_name_;
#endif

  int message_id_;
};

#endif  // CHROME_BROWSER_DEVICE_NOTIFICATIONS_DEVICE_PINNED_NOTIFICATION_RENDERER_H_
