// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/usb_printer_notification.h"

#include <memory>

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/experiences/settings_ui/settings_app_manager.h"
#include "components/user_manager/user.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "url/gurl.h"

namespace ash {

namespace {

const char kNotifierId[] = "printing.usb_printer";

}  // namespace

UsbPrinterNotification::UsbPrinterNotification(
    const chromeos::Printer& printer,
    const std::string& notification_id,
    Type type,
    const user_manager::User& user)
    : printer_(printer),
      notification_id_(CreateUserScopedNotificationId(notification_id,
                                                      user.username_hash())),
      type_(type),
      user_(user) {
  ShowNotification();
}

UsbPrinterNotification::~UsbPrinterNotification() = default;

void UsbPrinterNotification::CloseNotification() {
  message_center::MessageCenter::Get()->RemoveNotification(notification_id_,
                                                           /*by_user=*/false);
}

void UsbPrinterNotification::Click(const std::optional<int>& button_index,
                                   const std::optional<std::u16string>& reply) {
  if (!button_index) {
    // Body of notification clicked.
    if (type_ == Type::kConfigurationRequired) {
      SettingsAppManager::Get()->Open(
          *user_,
          {.sub_page = chromeos::settings::mojom::kPrintingDetailsSubpagePath});
    }
    return;
  }

  NOTREACHED();
}

void UsbPrinterNotification::ShowNotification() {
  message_center::NotifierId notifier_id(
      message_center::NotifierType::SYSTEM_COMPONENT, kNotifierId,
      NotificationCatalogName::kUsbPrinter);
  notifier_id.profile_id = user_->GetAccountId().GetUserEmail();

  std::u16string title;
  std::u16string message;
  switch (type_) {
    case Type::kEphemeral:
    case Type::kSaved:
      title = l10n_util::GetStringUTF16(
          IDS_USB_PRINTER_NOTIFICATION_CONNECTED_TITLE);
      message = l10n_util::GetStringFUTF16(
          IDS_USB_PRINTER_NOTIFICATION_CONNECTED_MESSAGE,
          base::UTF8ToUTF16(printer_.display_name()));
      break;
    case Type::kConfigurationRequired:
      title = l10n_util::GetStringUTF16(
          IDS_USB_PRINTER_NOTIFICATION_CONFIGURATION_REQUIRED_TITLE);
      message = l10n_util::GetStringFUTF16(
          IDS_USB_PRINTER_NOTIFICATION_CONFIGURATION_REQUIRED_MESSAGE,
          base::UTF8ToUTF16(printer_.display_name()));
      break;
  }

  message_center::MessageCenter::Get()->AddNotification(
      CreateSystemNotificationPtr(
          message_center::NOTIFICATION_TYPE_SIMPLE, notification_id_, title,
          message,
          l10n_util::GetStringUTF16(IDS_PRINT_JOB_NOTIFICATION_DISPLAY_SOURCE),
          /*origin_url=*/GURL(), notifier_id,
          message_center::RichNotificationData(),
          base::MakeRefCounted<message_center::ThunkNotificationDelegate>(
              weak_factory_.GetWeakPtr()),
          kNotificationPrintingIcon,
          message_center::SystemNotificationWarningLevel::NORMAL));
}

}  // namespace ash
