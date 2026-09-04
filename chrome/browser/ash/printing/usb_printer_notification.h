// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_USB_PRINTER_NOTIFICATION_H_
#define CHROME_BROWSER_ASH_PRINTING_USB_PRINTER_NOTIFICATION_H_

#include <optional>
#include <string>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/printing/printer_configuration.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

namespace user_manager {
class User;
}  // namespace user_manager

namespace ash {

// UsbPrinterNotification is used to create and manage the notification for a
// USB printer and respond to the user's action.
class UsbPrinterNotification : public message_center::NotificationObserver {
 public:
  enum class Type { kEphemeral, kSaved, kConfigurationRequired };

  UsbPrinterNotification(const chromeos::Printer& printer,
                         const std::string& notification_id,
                         Type type,
                         const user_manager::User& user);

  UsbPrinterNotification(const UsbPrinterNotification&) = delete;
  UsbPrinterNotification& operator=(const UsbPrinterNotification&) = delete;

  virtual ~UsbPrinterNotification();

  // Closes the notification, removing it from the notification tray.
  void CloseNotification();

  // message_center::NotificationObserver:
  void Click(const std::optional<int>& button_index,
             const std::optional<std::u16string>& reply) override;

 private:
  void ShowNotification();

  const chromeos::Printer printer_;
  const std::string notification_id_;
  const Type type_;
  const raw_ref<const user_manager::User> user_;

  base::WeakPtrFactory<UsbPrinterNotification> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_USB_PRINTER_NOTIFICATION_H_
