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
#include "base/check_is_test.h"
#include "chrome/browser/ash/notifications/update_notification_showing_controller.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"

namespace ash {
namespace {

constexpr char kUpdateNotificationId[] = "chrome://update_notification";

constexpr char kUpdateURL[] =
    "https://www.google.com/chromebook/discover/chromebookplus/";

}  // namespace

UpdateNotification::UpdateNotification(
    Profile* profile,
    UpdateNotificationShowingController* controller)
    : profile_(profile), controller_(controller) {
  CHECK(controller_);
  if (!profile_) {
    CHECK_IS_TEST();
  }
}

UpdateNotification::~UpdateNotification() = default;

void UpdateNotification::ShowNotification() {
  message_center::RichNotificationData data;
  data.buttons.emplace_back(l10n_util::GetStringUTF16(IDS_LEARN_MORE));

  message_center::Notification notification = CreateSystemNotification(
      message_center::NOTIFICATION_TYPE_IMAGE, kUpdateNotificationId,
      // Product name does not need to be translated.
      l10n_util::GetStringFUTF16(IDS_UPDATE_NOTIFICATION_HEADER,
                                 u"Chromebook Plus"),
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

  // DarkLightModeController might be nullptr in tests.
  auto* dark_light_mode_controller = DarkLightModeController::Get();
  const bool use_dark_image = dark_light_mode_controller &&
                              dark_light_mode_controller->IsDarkModeEnabled();
  int image_resource_id =
      use_dark_image ? IDR_UPDATE_CHROME_DARK : IDR_UPDATE_CHROME_LIGHT;

  notification.set_image(
      ui::ImageModel::FromResourceId(image_resource_id).GetImage());
  notification.set_priority(message_center::DEFAULT_PRIORITY);
  notification.set_pinned(false);

  NotificationDisplayServiceFactory::GetForProfile(profile_)->Display(
      NotificationHandler::Type::TRANSIENT, notification, /*metadata=*/nullptr);
  controller_->MarkNotificationShown();
}

void UpdateNotification::OnNotificationClick(absl::optional<int> button_index) {
  if (!button_index || !profile_) {
    return;
  }

  if (button_index == 0) {
    // Load the page in a new tab.
    NavigateParams params(profile_, GURL(kUpdateURL), ui::PAGE_TRANSITION_LINK);
    params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
    params.window_action = NavigateParams::SHOW_WINDOW;
    Navigate(&params);
  }
}

}  // namespace ash
