// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_notification_helper.h"

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_policy_constants.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#include "url/gurl.h"

namespace policy {

namespace {

constexpr char kPrintBlockedNotificationId[] = "print_dlp_blocked";
constexpr char kScreenShareBlockedNotificationId[] = "screen_share_dlp_blocked";
constexpr char kScreenSharePausedNotificationPrefix[] =
    "screen_share_dlp_paused-";
constexpr char kScreenShareResumedNotificationPrefix[] =
    "screen_share_dlp_resumed-";
constexpr char kScreenCaptureBlockedNotificationId[] =
    "screen_capture_dlp_blocked";
constexpr char kVideoCaptureStoppedNotificationId[] =
    "video_capture_dlp_stopped";
constexpr char kDlpPolicyNotifierId[] = "policy.dlp";

void OnNotificationClicked(const std::string& id) {
  ash::NewWindowDelegate::GetInstance()->OpenUrl(
      GURL(dlp::kDlpLearnMoreUrl),
      ash::NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      ash::NewWindowDelegate::Disposition::kNewForegroundTab);

  message_center::MessageCenter::Get()->RemoveNotification(id,
                                                           /*by_user=*/false);
}

void ShowDlpNotification(const std::string& id,
                         const std::u16string& title,
                         const std::u16string& message) {
  auto notification = std::make_unique<message_center::Notification>(
      message_center::NOTIFICATION_TYPE_SIMPLE, id, title, message,
      /*icon=*/ui::ImageModel(), /*display_source=*/std::u16string(),
      /*origin_url=*/GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kDlpPolicyNotifierId,
                                 ash::NotificationCatalogName::kDlpPolicy),
      message_center::RichNotificationData(),
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating(&OnNotificationClicked, id)));
  // Set critical warning color.
  notification->set_accent_color_id(ui::kColorSysError);
  notification->set_vector_small_image(features::IsRoundedIconsEnabled()
                                           ? vector_icons::kDomainIcon
                                           : vector_icons::kBusinessOldIcon);
  notification->set_renotify(true);
  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification));
}

std::string GetScreenSharePausedNotificationId(const std::string& share_id) {
  return kScreenSharePausedNotificationPrefix + share_id;
}

std::string GetScreenShareResumedNotificationId(const std::string& share_id) {
  return kScreenShareResumedNotificationPrefix + share_id;
}

}  // namespace

void ShowDlpPrintDisabledNotification() {
  ShowDlpNotification(
      kPrintBlockedNotificationId,
      l10n_util::GetStringUTF16(IDS_POLICY_DLP_PRINTING_BLOCKED_TITLE),
      l10n_util::GetStringUTF16(IDS_POLICY_DLP_PRINTING_BLOCKED_MESSAGE));
}

void ShowDlpScreenShareDisabledNotification(const std::u16string& app_title) {
  ShowDlpNotification(
      kScreenShareBlockedNotificationId,
      l10n_util::GetStringUTF16(IDS_POLICY_DLP_SCREEN_SHARE_BLOCKED_TITLE),
      l10n_util::GetStringFUTF16(IDS_POLICY_DLP_SCREEN_SHARE_BLOCKED_MESSAGE,
                                 app_title));
}

void HideDlpScreenSharePausedNotification(const std::string& share_id) {
  message_center::MessageCenter::Get()->RemoveNotification(
      GetScreenSharePausedNotificationId(share_id), /*by_user=*/false);
}

void ShowDlpScreenSharePausedNotification(const std::string& share_id,
                                          const std::u16string& app_title) {
  ShowDlpNotification(
      GetScreenSharePausedNotificationId(share_id),
      l10n_util::GetStringUTF16(IDS_POLICY_DLP_SCREEN_SHARE_PAUSED_TITLE),
      l10n_util::GetStringFUTF16(IDS_POLICY_DLP_SCREEN_SHARE_PAUSED_MESSAGE,
                                 app_title));
}

void HideDlpScreenShareResumedNotification(const std::string& share_id) {
  message_center::MessageCenter::Get()->RemoveNotification(
      GetScreenShareResumedNotificationId(share_id), /*by_user=*/false);
}

void ShowDlpScreenShareResumedNotification(const std::string& share_id,
                                           const std::u16string& app_title) {
  ShowDlpNotification(
      GetScreenShareResumedNotificationId(share_id),
      l10n_util::GetStringUTF16(IDS_POLICY_DLP_SCREEN_SHARE_RESUMED_TITLE),
      l10n_util::GetStringFUTF16(IDS_POLICY_DLP_SCREEN_SHARE_RESUMED_MESSAGE,
                                 app_title));
}

void ShowDlpScreenCaptureDisabledNotification() {
  ShowDlpNotification(
      kScreenCaptureBlockedNotificationId,
      l10n_util::GetStringUTF16(IDS_POLICY_DLP_SCREEN_CAPTURE_DISABLED_TITLE),
      l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_SCREEN_CAPTURE_DISABLED_MESSAGE));
}

void ShowDlpVideoCaptureStoppedNotification() {
  ShowDlpNotification(
      kVideoCaptureStoppedNotificationId,
      l10n_util::GetStringUTF16(IDS_POLICY_DLP_VIDEO_CAPTURE_STOPPED_TITLE),
      l10n_util::GetStringUTF16(IDS_POLICY_DLP_VIDEO_CAPTURE_STOPPED_MESSAGE));
}

}  // namespace policy
