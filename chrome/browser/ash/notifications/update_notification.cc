// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/notifications/update_notification.h"

#include <memory>
#include <string>

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/resources/grit/ash_public_unscaled_resources.h"
#include "ash/public/cpp/style/dark_light_mode_controller.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/message_center/public/cpp/notification.h"

namespace ash {
namespace {

constexpr char kUpdateNotificationId[] = "chrome://update_notification";

// TODO(b/284978852): Update the link.
constexpr char kUpdateURL[] = "https://www.google.com/chromebook/";

}  // namespace

UpdateNotification::UpdateNotification() = default;

UpdateNotification::~UpdateNotification() = default;

void UpdateNotification::ShowNotification() {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();

  message_center::RichNotificationData data;
  data.buttons.emplace_back(l10n_util::GetStringUTF16(IDS_LEARN_MORE));

  // Product name does not need to be translated.
  auto product_name =
      l10n_util::GetStringUTF16(ui::GetChromeOSDeviceTypeResourceId());
  message_center::Notification notification = CreateSystemNotification(
      message_center::NOTIFICATION_TYPE_IMAGE, kUpdateNotificationId,
      l10n_util::GetStringFUTF16(IDS_UPDATE_NOTIFICATION_HEADER, product_name),
      l10n_util::GetStringUTF16(IDS_UPDATE_NOTIFICATION_MESSAGE),
      std::u16string(), GURL(kUpdateNotificationId),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kUpdateNotificationId,
                                 NotificationCatalogName::kUpdateNotification),
      data,
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating(&UpdateNotification::OnNotificationClick,
                              weak_factory_.GetWeakPtr())),
      gfx::kNoneIcon, message_center::SystemNotificationWarningLevel::NORMAL);
  // TODO(b/284978852): Use the images after it's finalized from the UX.
  int image_resource_id = DarkLightModeController::Get()->IsDarkModeEnabled()
                              ? IDR_TRAY_CAST_ZERO_STATE_DARK
                              : IDR_TRAY_CAST_ZERO_STATE_LIGHT;
  notification.set_image(
      ui::ImageModel::FromResourceId(image_resource_id).GetImage());
  notification.SetSystemPriority();
  notification.set_pinned(false);

  NotificationDisplayServiceFactory::GetForProfile(profile)->Display(
      NotificationHandler::Type::TRANSIENT, notification, /*metadata=*/nullptr);
}

void UpdateNotification::OnNotificationClick(absl::optional<int> button_index) {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  if (!button_index || !profile) {
    return;
  }

  if (button_index == 0) {
    // Load the page in a new tab.
    NavigateParams params(profile, GURL(kUpdateURL), ui::PAGE_TRANSITION_LINK);
    params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
    params.window_action = NavigateParams::SHOW_WINDOW;
    Navigate(&params);
  }
}

}  // namespace ash
