// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_notification_helper.h"

#include "build/chromeos_buildflags.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_policy_constants.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/new_window_delegate.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

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

void OnNotificationClicked(const std::string id) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::NewWindowDelegate::GetPrimary()->OpenUrl(
      GURL(dlp::kDlpLearnMoreUrl),
      ash::NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      ash::NewWindowDelegate::Disposition::kNewForegroundTab);
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  // The dlp policy applies to the main profile, so use the main profile for
  // opening the page.
  NavigateParams navigate_params(
      ProfileManager::GetPrimaryUserProfile(), GURL(dlp::kDlpLearnMoreUrl),
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_FROM_API));
  navigate_params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  navigate_params.window_action = NavigateParams::SHOW_WINDOW;
  Navigate(&navigate_params);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  NotificationDisplayServiceFactory::GetForProfile(
      ProfileManager::GetActiveUserProfile())
      ->Close(NotificationHandler::Type::TRANSIENT, id);
}

void ShowDlpNotification(const std::string& id,
                         const std::u16string& title,
                         const std::u16string& message) {
  message_center::Notification notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, id, title, message,
      /*icon=*/ui::ImageModel(), /*display_source=*/std::u16string(),
      /*origin_url=*/GURL(),
#if BUILDFLAG(IS_CHROMEOS_ASH)
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kDlpPolicyNotifierId,
                                 ash::NotificationCatalogName::kDlpPolicy),
#else
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kDlpPolicyNotifierId),
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
      message_center::RichNotificationData(),
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating(&OnNotificationClicked, id)));
  // Set critical warning color.
  notification.set_accent_color_id(ui::kColorSysError);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  notification.set_system_notification_warning_level(
      message_center::SystemNotificationWarningLevel::CRITICAL_WARNING);
#endif
  notification.set_vector_small_image(vector_icons::kBusinessIcon);
  notification.set_renotify(true);
  NotificationDisplayServiceFactory::GetForProfile(
      ProfileManager::GetActiveUserProfile())
      ->Display(NotificationHandler::Type::TRANSIENT, notification,
                /*metadata=*/nullptr);
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
  auto* notification_display_service =
      NotificationDisplayServiceFactory::GetForProfile(
          ProfileManager::GetActiveUserProfile());
  if (notification_display_service) {
    notification_display_service->Close(
        NotificationHandler::Type::TRANSIENT,
        GetScreenSharePausedNotificationId(share_id));
  }
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
  auto* notification_display_service =
      NotificationDisplayServiceFactory::GetForProfile(
          ProfileManager::GetActiveUserProfile());
  if (notification_display_service) {
    notification_display_service->Close(
        NotificationHandler::Type::TRANSIENT,
        GetScreenShareResumedNotificationId(share_id));
  }
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
