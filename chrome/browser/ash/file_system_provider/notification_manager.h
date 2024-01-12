// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_NOTIFICATION_MANAGER_H_
#define CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_NOTIFICATION_MANAGER_H_

#include <map>
#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/file_system_provider/notification_manager_interface.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/ui/app_icon_loader.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

class Profile;

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace ash::file_system_provider {

// Provided file systems's manager for showing notifications. Shows always
// up to one notification. If more than one request is unresponsive, then
// all of them will be aborted when clicking on the notification button.
class NotificationManager : public NotificationManagerInterface,
                            public AppIconLoaderDelegate,
                            public message_center::NotificationObserver {
 public:
  NotificationManager(Profile* profile,
                      const ProvidedFileSystemInfo& file_system_info);

  NotificationManager(const NotificationManager&) = delete;
  NotificationManager& operator=(const NotificationManager&) = delete;

  ~NotificationManager() override;

  // NotificationManagerInterface overrides:
  void ShowUnresponsiveNotification(int id,
                                    NotificationCallback callback) override;
  void HideUnresponsiveNotification(int id) override;

  // AppIconLoaderDelegate overrides:
  void OnAppImageUpdated(
      const std::string& id,
      const gfx::ImageSkia& image,
      bool is_placeholder_icon,
      const std::optional<gfx::ImageSkia>& badge_image) override;

  // message_center::NotificationObserver overrides:
  void Click(const std::optional<int>& button_index,
             const std::optional<std::u16string>& reply) override;
  void Close(bool by_user) override;

 private:
  typedef std::map<int, NotificationCallback> CallbackMap;

  std::string GetNotificationId();

  // Creates and displays a notification object for the actual state of the
  // manager. This will either add a new one or update the existing
  // notification.
  void ShowNotification();

  // Handles a notification result by calling all registered callbacks and
  // clearing the list.
  void OnNotificationResult(NotificationResult result);

  raw_ptr<Profile> profile_;
  ProvidedFileSystemInfo file_system_info_;
  CallbackMap callbacks_;
  std::unique_ptr<AppIconLoader> icon_loader_;
  ui::ImageModel extension_icon_;
  base::WeakPtrFactory<NotificationManager> weak_factory_{this};
};

}  // namespace ash::file_system_provider

#endif  // CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_NOTIFICATION_MANAGER_H_
