// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/skyvault/signin_notification_helper.h"

#include <string>

#include "ash/public/cpp/notification_utils.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_util.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/message_center/public/cpp/notification.h"

namespace policy::skyvault_ui_utils {

namespace {

class SignInNotificationDelegate : public message_center::NotificationDelegate {
 public:
  SignInNotificationDelegate(
      Profile* profile,
      const std::string& notification_id,
      base::OnceCallback<void(base::File::Error)> signin_callback);

  SignInNotificationDelegate(const SignInNotificationDelegate&) = delete;
  SignInNotificationDelegate& operator=(const SignInNotificationDelegate&) =
      delete;

 protected:
  ~SignInNotificationDelegate() override;

  // message_center::NotificationDelegate overrides:
  void Close(bool by_user) override;
  void Click(const std::optional<int>& button_index,
             const std::optional<std::u16string>& reply) override;

 private:
  raw_ptr<Profile> profile_;
  const std::string notification_id_;
  // Should be run with the sign-in result.
  base::OnceCallback<void(base::File::Error)> signin_callback_;
};

SignInNotificationDelegate::SignInNotificationDelegate(
    Profile* profile,
    const std::string& notification_id,
    base::OnceCallback<void(base::File::Error)> signin_callback)
    : profile_(profile),
      notification_id_(notification_id),
      signin_callback_(std::move(signin_callback)) {}

SignInNotificationDelegate::~SignInNotificationDelegate() {
  if (signin_callback_) {
    // Notification is destroyed but the user didn't click on anything.
    std::move(signin_callback_).Run(base::File::Error::FILE_ERROR_FAILED);
  }
}

void SignInNotificationDelegate::Close(bool by_user) {
  if (signin_callback_) {
    std::move(signin_callback_).Run(base::File::Error::FILE_ERROR_FAILED);
  }
}

void SignInNotificationDelegate::Click(
    const std::optional<int>& button_index,
    const std::optional<std::u16string>& reply) {
  if (!signin_callback_) {
    return;
  }
  if (!button_index) {
    return;
  }

  switch (*button_index) {
    case NotificationButtonIndex::kSignInButton:
      // Request an ODFS mount which will trigger reauthentication.
      ash::cloud_upload::RequestODFSMount(profile_,
                                          std::move(signin_callback_));
      break;
    case NotificationButtonIndex::kCancelButton:
      std::move(signin_callback_).Run(base::File::Error::FILE_ERROR_FAILED);
      break;
  }
  NotificationDisplayService* notification_service =
      NotificationDisplayServiceFactory::GetForProfile(profile_);
  notification_service->Close(NotificationHandler::Type::TRANSIENT,
                              notification_id_);
}

}  // namespace

void ShowSignInNotification(
    Profile* profile,
    int64_t id,
    local_user_files::UploadTrigger trigger,
    const base::FilePath& file_path,
    base::OnceCallback<void(base::File::Error)> signin_callback,
    std::optional<const gfx::Image> thumbnail) {
  switch (trigger) {
    case local_user_files::UploadTrigger::kDownload: {
      message_center::RichNotificationData rich_notification_data;
      rich_notification_data.should_make_spoken_feedback_for_popup_updates =
          false;
      rich_notification_data.vector_small_image =
          &vector_icons::kNotificationDownloadIcon;
      // TODO(b/356326503): Fix the strings.
      auto notification_id = base::StrCat(
          {kDownloadSignInNotificationPrefix, base::NumberToString(id)});
      message_center::Notification notification(
          message_center::NOTIFICATION_TYPE_SIMPLE, notification_id,
          /*title=*/
          l10n_util::GetStringUTF16(IDS_POLICY_SKYVAULT_DOWNLOAD_SIGN_IN_TITLE),
          /*message=*/base::UTF8ToUTF16(file_path.BaseName().value()),
          /*icon=*/ui::ImageModel(),
          /*display_source=*/
          l10n_util::GetStringUTF16(
              IDS_POLICY_SKYVAULT_DOWNLOAD_SIGN_IN_DISPLAY_SOURCE),
          /*origin_url=*/GURL(),
          /*notifier_id=*/message_center::NotifierId(), rich_notification_data,
          base::MakeRefCounted<SignInNotificationDelegate>(
              profile, notification_id, std::move(signin_callback)));
      notification.set_fullscreen_visibility(
          message_center::FullscreenVisibility::OVER_USER);
      notification.set_accent_color(
          ash::kSystemNotificationColorCriticalWarning);
      notification.set_accent_color_id(cros_tokens::kColorAlert);

      message_center::ButtonInfo signin_button(l10n_util::GetStringUTF16(
          IDS_POLICY_SKYVAULT_DOWNLOAD_SIGN_IN_BUTTON));
      message_center::ButtonInfo cancel_button(l10n_util::GetStringUTF16(
          IDS_POLICY_SKYVAULT_DOWNLOAD_SIGN_IN_CANCEL_BUTTON));
      notification.set_buttons({signin_button, cancel_button});

      NotificationDisplayServiceFactory::GetForProfile(profile)->Display(
          NotificationHandler::Type::TRANSIENT, notification,
          /*metadata=*/nullptr);

      break;
    }
    case local_user_files::UploadTrigger::kScreenCapture: {
      message_center::RichNotificationData rich_notification_data;
      rich_notification_data.vector_small_image = &ash::kCaptureModeIcon;
      if (thumbnail.has_value()) {
        rich_notification_data.image = thumbnail.value();
        rich_notification_data.image_path = file_path;
      }
      auto notification_id = base::StrCat(
          {kScreenCaptureSignInNotificationIdPrefix, base::NumberToString(id)});
      message_center::Notification notification(
          message_center::NOTIFICATION_TYPE_SIMPLE, notification_id,
          /*title=*/
          l10n_util::GetStringUTF16(
              IDS_POLICY_SKYVAULT_SCREENCAPTURE_SIGN_IN_TITLE),
          /*message=*/std::u16string(),
          /*icon=*/ui::ImageModel(),
          /*display_source=*/
          l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_DISPLAY_SOURCE),
          /*origin_url=*/GURL(),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT, notification_id,
              ash::NotificationCatalogName::kScreenCapture),
          rich_notification_data,
          base::MakeRefCounted<SignInNotificationDelegate>(
              profile, notification_id, std::move(signin_callback)));

      notification.set_fullscreen_visibility(
          message_center::FullscreenVisibility::OVER_USER);
      notification.set_accent_color(
          ash::kSystemNotificationColorCriticalWarning);
      notification.set_accent_color_id(cros_tokens::kColorAlert);

      message_center::ButtonInfo signin_button(l10n_util::GetStringUTF16(
          IDS_POLICY_SKYVAULT_SCREENCAPTURE_SIGN_IN_BUTTON));
      message_center::ButtonInfo cancel_button(l10n_util::GetStringUTF16(
          IDS_POLICY_SKYVAULT_SCREENCAPTURE_SIGN_IN_CANCEL_BUTTON));
      notification.set_buttons({signin_button, cancel_button});

      NotificationDisplayServiceFactory::GetForProfile(profile)->Display(
          NotificationHandler::Type::TRANSIENT, notification,
          /*metadata=*/nullptr);

      break;
    }
    case local_user_files::UploadTrigger::kMigration: {
      message_center::RichNotificationData optional_fields;
      optional_fields.never_timeout = true;
      auto notification = ash::CreateSystemNotificationPtr(
          message_center::NotificationType::NOTIFICATION_TYPE_SIMPLE,
          kMigrationSignInNotification,
          l10n_util::GetStringUTF16(
              IDS_POLICY_SKYVAULT_MIGRATION_SIGN_IN_TITLE),
          l10n_util::GetStringUTF16(
              IDS_POLICY_SKYVAULT_MIGRATION_SIGN_IN_MESSAGE),
          /*display_source=*/std::u16string(), /*origin_url=*/GURL(),
          message_center::NotifierId(), optional_fields,
          base::MakeRefCounted<SignInNotificationDelegate>(
              profile, kMigrationSignInNotification,
              std::move(signin_callback)),
          vector_icons::kBusinessIcon,
          message_center::SystemNotificationWarningLevel::NORMAL);

      notification->set_fullscreen_visibility(
          message_center::FullscreenVisibility::OVER_USER);
      notification->set_accent_color(
          ash::kSystemNotificationColorCriticalWarning);
      notification->set_accent_color_id(cros_tokens::kColorAlert);

      notification->set_buttons(
          {message_center::ButtonInfo(l10n_util::GetStringUTF16(
              IDS_POLICY_SKYVAULT_MIGRATION_SIGN_IN_BUTTON))});

      NotificationDisplayServiceFactory::GetForProfile(profile)->Display(
          NotificationHandler::Type::TRANSIENT, *notification,
          /*metadata=*/nullptr);
      break;
    }
  }
}

}  // namespace policy::skyvault_ui_utils
