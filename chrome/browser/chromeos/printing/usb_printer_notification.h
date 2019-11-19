// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_USB_PRINTER_NOTIFICATION_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_USB_PRINTER_NOTIFICATION_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/printing/printer_configurer.h"
#include "chromeos/printing/printer_configuration.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

class Profile;

namespace message_center {
class Notification;
}

namespace chromeos {

// UsbPrinterNotification is used to update the notification of a print job
// according to its state and respond to the user's action.
class UsbPrinterNotification : public message_center::NotificationObserver {
 public:
  enum class Type { kEphemeral, kSaved, kConfigurationRequired };

  UsbPrinterNotification(const Printer& printer,
                         const std::string& notification_id,
                         Type type,
                         Profile* profile);

  virtual ~UsbPrinterNotification();

  // Closes the notification, removing it from the notification tray.
  void CloseNotification();

  // message_center::NotificationObserver
  void Close(bool by_user) override;
  void Click(const base::Optional<int>& button_index,
             const base::Optional<base::string16>& reply) override;

 private:
  void UpdateContents();

  void ShowNotification();

  const Printer printer_;
  std::string notification_id_;
  Type type_;
  Profile* profile_;  // Not owned.
  std::unique_ptr<message_center::Notification> notification_;
  bool visible_;

  base::WeakPtrFactory<UsbPrinterNotification> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(UsbPrinterNotification);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_USB_PRINTER_NOTIFICATION_H_
