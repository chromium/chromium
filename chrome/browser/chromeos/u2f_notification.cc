// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/u2f_notification.h"

#include <memory>

#include "ash/public/cpp/notification_utils.h"
#include "base/bind.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/task/post_task.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon/debug_daemon_client.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/cros_system_api/dbus/debugd/dbus-constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification.h"

namespace chromeos {
namespace {

constexpr char kU2FNotificationId[] = "chrome://u2f_notification";
constexpr char kU2FAdvisoryURL[] =
    "https://sites.google.com/a/chromium.org/dev/chromium-os/"
    "u2f-ecdsa-vulnerability";

// Notification button identifiers.
enum class ButtonIndex : int {
  kLearnMore = 0,
  kReset = 1,
};

}  // namespace

U2FNotification::U2FNotification() {}

U2FNotification::~U2FNotification() {}

void U2FNotification::Check() {
  DBusThreadManager::Get()->GetDebugDaemonClient()->GetU2fFlags(base::BindOnce(
      &U2FNotification::CheckStatus, weak_factory_.GetWeakPtr()));
}

void U2FNotification::CheckStatus(base::Optional<std::set<std::string>> flags) {
  if (!flags) {
    LOG(ERROR) << "Failed to get U2F flags.";
    return;
  }

  // The legacy implementation is only enabled if either the U2F or G2F flags
  // are present and the user_keys flag is off (the latter enables the improved
  // implementation).
  if (!(base::Contains(*flags, debugd::u2f_flags::kU2f) ||
        base::Contains(*flags, debugd::u2f_flags::kG2f)) ||
      base::Contains(*flags, debugd::u2f_flags::kUserKeys)) {
    return;
  }

  CrosSettings* settings = CrosSettings::Get();
  switch (settings->PrepareTrustedValues(base::BindRepeating(
      &U2FNotification::CheckStatus, weak_factory_.GetWeakPtr(), flags))) {
    case CrosSettingsProvider::TEMPORARILY_UNTRUSTED:
      // Retry happens via the callback registered above.
      return;
    case CrosSettingsProvider::PERMANENTLY_UNTRUSTED:
      // No device settings, so they won't take effect.
      break;
    case CrosSettingsProvider::TRUSTED:
      // If the 2FA setting is present and not set to "unset", it disables the
      // legacy implementation, so bail if that is the case. This corresponds
      // with behavior in u2fd and should be kept in sync.
      constexpr auto kU2fModeUnset =
          enterprise_management::DeviceSecondFactorAuthenticationProto::UNSET;
      int mode;
      if (settings->GetInteger(kDeviceSecondFactorAuthenticationMode, &mode) &&
          mode != kU2fModeUnset) {
        return;
      }
      break;
  }

  // Legacy implementation is on, notify.
  ShowNotification();
}

void U2FNotification::ShowNotification() {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();

  message_center::RichNotificationData data;
  data.buttons.emplace_back(l10n_util::GetStringUTF16(IDS_LEARN_MORE));
  data.buttons.emplace_back(
      l10n_util::GetStringUTF16(IDS_U2F_INSECURE_NOTIFICATION_RESET));
  std::unique_ptr<message_center::Notification> notification =
      ash::CreateSystemNotification(
          message_center::NOTIFICATION_TYPE_SIMPLE, kU2FNotificationId,
          l10n_util::GetStringUTF16(IDS_U2F_INSECURE_NOTIFICATION_TITLE),
          l10n_util::GetStringUTF16(IDS_U2F_INSECURE_NOTIFICATION_MESSAGE),
          base::string16(), GURL(kU2FNotificationId),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT,
              kU2FNotificationId),
          data,
          base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
              base::BindRepeating(&U2FNotification::OnNotificationClick,
                                  weak_factory_.GetWeakPtr())),
          gfx::kNoneIcon,
          message_center::SystemNotificationWarningLevel::WARNING);
  notification->SetSystemPriority();
  notification->set_pinned(false);

  NotificationDisplayServiceFactory::GetForProfile(profile)->Display(
      NotificationHandler::Type::TRANSIENT, *notification,
      nullptr /* metadata */);
}

void U2FNotification::OnNotificationClick(
    const base::Optional<int> button_index) {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  if (!button_index || !profile) {
    return;
  }

  switch (static_cast<ButtonIndex>(*button_index)) {
    case ButtonIndex::kLearnMore: {
      // Load the chromium.org advisory page in a new tab.
      NavigateParams params(profile, GURL(kU2FAdvisoryURL),
                            ui::PAGE_TRANSITION_LINK);
      params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
      params.window_action = NavigateParams::SHOW_WINDOW;
      Navigate(&params);
      break;
    }
    case ButtonIndex::kReset: {
      // Add the user_keys flag.
      DBusThreadManager::Get()->GetDebugDaemonClient()->GetU2fFlags(
          base::BindOnce([](base::Optional<std::set<std::string>> flags) {
            if (!flags) {
              LOG(ERROR) << "Failed to get U2F flags.";
              return;
            }
            flags->insert(debugd::u2f_flags::kUserKeys);
            DBusThreadManager::Get()->GetDebugDaemonClient()->SetU2fFlags(
                *flags, base::BindOnce([](bool result) {
                  if (!result) {
                    LOG(ERROR) << "Failed to set U2F flags.";
                    return;
                  }
                }));
          }));

      // TODO: Should we close in all cases?
      NotificationDisplayServiceFactory::GetForProfile(profile)->Close(
          NotificationHandler::Type::TRANSIENT, kU2FNotificationId);
      break;
    }
  }
}

}  // namespace chromeos
