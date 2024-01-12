// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_USB_PRINTER_NOTIFICATION_H_
#define CHROME_BROWSER_ASH_PRINTING_USB_PRINTER_NOTIFICATION_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/printing/printer_configurer.h"
#include "chromeos/printing/printer_configuration.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

class Profile;

namespace message_center {
class Notification;
}  // namespace message_center

namespace ash {

// UsbPrinterNotification is used to update the notification of a print job
// according to its state and respond to the user's action.
class UsbPrinterNotification : public message_center::NotificationObserver {
 public:
  enum class Type { kEphemeral, kSaved, kConfigurationRequired };

  UsbPrinterNotification(const chromeos::Printer& printer,
                         const std::string& notification_id,
                         Type type,
                         Profile* profile);

  UsbPrinterNotification(const UsbPrinterNotification&) = delete;
  UsbPrinterNotification& operator=(const UsbPrinterNotification&) = delete;

  virtual ~UsbPrinterNotification();

  // Closes the notification, removing it from the notification tray.
  void CloseNotification();

  // message_center::NotificationObserver
  void Close(bool by_user) override;
  void Click(const std::optional<int>& button_index,
             const std::optional<std::u16string>& reply) override;

 private:
  void UpdateContents();

  void ShowNotification();

  const chromeos::Printer printer_;
  std::string notification_id_;
  Type type_;
  raw_ptr<Profile> profile_;  // Not owned.
  std::unique_ptr<message_center::Notification> notification_;
  bool visible_;

  base::WeakPtrFactory<UsbPrinterNotification> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_USB_PRINTER_NOTIFICATION_H_
