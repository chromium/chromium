// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/help_app/help_app_discover_tab_notification.h"

#include "ash/public/cpp/notification_utils.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

HelpAppDiscoverTabNotification::HelpAppDiscoverTabNotification(Profile* profile)
    : profile_(profile) {}

HelpAppDiscoverTabNotification::~HelpAppDiscoverTabNotification() = default;

void HelpAppDiscoverTabNotification::Show() {
  std::u16string title =
      l10n_util::GetStringUTF16(IDS_HELP_APP_DISCOVER_TAB_NOTIFICATION_TITLE);
  std::u16string message =
      l10n_util::GetStringUTF16(IDS_HELP_APP_DISCOVER_TAB_NOTIFICATION_MESSAGE);

  message_center::Notification notification = CreateSystemNotification(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      kShowHelpAppDiscoverTabNotificationId, std::move(title),
      std::move(message), l10n_util::GetStringUTF16(IDS_HELP_APP_EXPLORE),
      GURL(), message_center::NotifierId(),
      message_center::RichNotificationData(),

      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating(&HelpAppDiscoverTabNotification::OnClick,
                              weak_ptr_factory_.GetWeakPtr())),
      kNotificationHelpAppIcon,
      message_center::SystemNotificationWarningLevel::NORMAL);

  SystemNotificationHelper::GetInstance()->Display(notification);

  base::RecordAction(
      base::UserMetricsAction("Discover.DiscoverTabNotification.Shown"));
}

void HelpAppDiscoverTabNotification::OnClick(absl::optional<int> button_index) {
  SystemNotificationHelper::GetInstance()->Close(
      kShowHelpAppDiscoverTabNotificationId);
  SystemAppLaunchParams params;
  params.url = GURL("chrome://help-app/discover");
  params.launch_source = apps::LaunchSource::kFromDiscoverTabNotification;
  LaunchSystemWebAppAsync(profile_, SystemWebAppType::HELP, params);

  base::RecordAction(
      base::UserMetricsAction("Discover.DiscoverTabNotification.Clicked"));

  if (onclick_callback_) {
    onclick_callback_.Run();
  }
}

void HelpAppDiscoverTabNotification::SetOnClickCallbackForTesting(
    base::RepeatingCallback<void()> callback) {
  onclick_callback_ = callback;
}

}  // namespace ash
