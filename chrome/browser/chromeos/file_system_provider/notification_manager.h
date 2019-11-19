// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_NOTIFICATION_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_NOTIFICATION_MANAGER_H_

#include <map>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/file_system_provider/notification_manager_interface.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/ui/app_icon_loader.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

class Profile;

namespace gfx {
class Image;
class ImageSkia;
}  // message gfx

namespace chromeos {
namespace file_system_provider {

// Provided file systems's manager for showing notifications. Shows always
// up to one notification. If more than one request is unresponsive, then
// all of them will be aborted when clicking on the notification button.
class NotificationManager : public NotificationManagerInterface,
                            public AppIconLoaderDelegate,
                            public message_center::NotificationObserver {
 public:
  NotificationManager(Profile* profile,
                      const ProvidedFileSystemInfo& file_system_info);
  ~NotificationManager() override;

  // NotificationManagerInterface overrides:
  void ShowUnresponsiveNotification(
      int id,
      const NotificationCallback& callback) override;
  void HideUnresponsiveNotification(int id) override;

  // AppIconLoaderDelegate overrides:
  void OnAppImageUpdated(const std::string& id,
                         const gfx::ImageSkia& image) override;

  // message_center::NotificationObserver overrides:
  void Click(const base::Optional<int>& button_index,
             const base::Optional<base::string16>& reply) override;
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

  Profile* profile_;
  ProvidedFileSystemInfo file_system_info_;
  CallbackMap callbacks_;
  std::unique_ptr<AppIconLoader> icon_loader_;
  gfx::Image extension_icon_;
  base::WeakPtrFactory<NotificationManager> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(NotificationManager);
};

}  // namespace file_system_provider
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_NOTIFICATION_MANAGER_H_
