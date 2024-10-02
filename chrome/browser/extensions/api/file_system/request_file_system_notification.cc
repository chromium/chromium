// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/file_system/request_file_system_notification.h"

#include <string>
#include <utility>

#include "ash/constants/notifier_catalogs.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/chrome_app_icon_loader.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_icon_loader.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/public/cpp/notifier_id.h"

using message_center::Notification;

namespace extensions {

namespace {

// Extension icon size for the notification.
const int kIconSize = 48;

// Loads an app's icon and uses it to display a notification.
class AppNotificationLauncher : public AppIconLoaderDelegate,
                                public message_center::NotificationDelegate {
 public:
  // This class owns and deletes itself after the shown notification closes.
  AppNotificationLauncher() = default;

  AppNotificationLauncher(const AppNotificationLauncher&) = delete;
  AppNotificationLauncher& operator=(const AppNotificationLauncher&) = delete;

  void InitAndShow(Profile* profile,
                   const extensions::ExtensionId& extension_id,
                   std::unique_ptr<message_center::Notification> notification) {
    profile_ = profile;
    pending_notification_ = std::move(notification);

    icon_loader_ =
        std::make_unique<ChromeAppIconLoader>(profile, kIconSize, this);
    icon_loader_->FetchImage(extension_id);
  }

  // AppIconLoaderDelegate overrides:
  // This is triggered from FetchImage() in InitAndShow(), and can be called
  // multiple times, synchronously or asynchronously.
  void OnAppImageUpdated(
      const std::string& id,
      const gfx::ImageSkia& image,
      bool is_placeholder_icon,
      const std::optional<gfx::ImageSkia>& badge_image) override {
    pending_notification_->set_icon(ui::ImageModel::FromImageSkia(image));
    auto* notification_display_service =
        NotificationDisplayServiceFactory::GetForProfile(profile_);

    notification_display_service->Display(NotificationHandler::Type::TRANSIENT,
                                          *pending_notification_,
                                          /*metadata=*/nullptr);
  }

  // message_center::NotificationDelegate override.
  void Close(bool by_user) override {
    // On notification close, free |pending_notification| (which holds a
    // reference to this) to trigger self-deletion.
    pending_notification_.reset();
  }

 private:
  ~AppNotificationLauncher() override = default;

  raw_ptr<Profile> profile_;
  std::unique_ptr<AppIconLoader> icon_loader_;
  std::unique_ptr<message_center::Notification> pending_notification_;
};

}  // namespace

void ShowNotificationForAutoGrantedRequestFileSystem(
    Profile* profile,
    const extensions::ExtensionId& extension_id,
    const std::string& extension_name,
    const std::string& volume_id,
    const std::string& volume_label,
    bool writable) {
  DCHECK(profile);
  static int sequence = 0;
  // Create globally unique |notification_id| so that notifications are not
  // suppressed, thus allowing each AppNotificationLauncher instance to
  // correspond to an actual notification, and properly deallocated on close.
  const std::string notification_id = base::StringPrintf(
      "%s-%s-%d", extension_id.c_str(), volume_id.c_str(), sequence);
  ++sequence;

  message_center::RichNotificationData data;

  // TODO(mtomasz): Share this code with RequestFileSystemDialogView.
  const std::u16string display_name =
      base::UTF8ToUTF16(volume_label.empty() ? volume_id : volume_label);
  const std::u16string message = l10n_util::GetStringFUTF16(
      writable
          ? IDS_FILE_SYSTEM_REQUEST_FILE_SYSTEM_NOTIFICATION_WRITABLE_MESSAGE
          : IDS_FILE_SYSTEM_REQUEST_FILE_SYSTEM_NOTIFICATION_MESSAGE,
      display_name);

  // Helper that self-deletes when the displayed notification closes.
  scoped_refptr<AppNotificationLauncher> app_notification_launcher =
      base::MakeRefCounted<AppNotificationLauncher>();

  std::unique_ptr<message_center::Notification> notification(new Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, notification_id,
      base::UTF8ToUTF16(extension_name), message,
      ui::ImageModel(),  // Updated asynchronously later.
      std::u16string(),  // display_source
      GURL(),
#if BUILDFLAG(IS_CHROMEOS_ASH)
      message_center::NotifierId(
          message_center::NotifierType::SYSTEM_COMPONENT, notification_id,
          ash::NotificationCatalogName::kRequestFileSystem),
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(IS_CHROMEOS_LACROS)
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 notification_id),
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
      data, app_notification_launcher));

  app_notification_launcher->InitAndShow(profile, extension_id,
                                         std::move(notification));
}

}  // namespace extensions
