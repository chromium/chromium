// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_USB_PRINTER_NOTIFICATION_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_USB_PRINTER_NOTIFICATION_CONTROLLER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "chrome/browser/chromeos/printing/usb_printer_notification.h"

class Profile;

namespace chromeos {

class Printer;

class UsbPrinterNotificationController {
 public:
  static std::unique_ptr<UsbPrinterNotificationController> Create(
      Profile* profile);

  virtual ~UsbPrinterNotificationController() = default;

  // Creates a notification for an ephemeral printer. This is a no-op if there
  // is already an existing notification for |printer|.
  virtual void ShowEphemeralNotification(const Printer& printer) = 0;

  // Creates a notification for a saved printer. This is a no-op if there
  // is already an existing notification for |printer|.
  virtual void ShowSavedNotification(const Printer& printer) = 0;

  // Creates a notification for a printer that needs configuration. This is a
  // no-op if there is already an existing notification for |printer|.
  virtual void ShowConfigurationNotification(const Printer& printer) = 0;

  // Closes the notification for |printer_id|. This is a no-op if the
  // notification has already been closed by the user.
  virtual void RemoveNotification(const std::string& printer_id) = 0;

  // Returns true if there is an existing notification for |printer_id|.
  virtual bool IsNotificationDisplayed(const std::string& printer_id) const = 0;

 protected:
  UsbPrinterNotificationController() = default;

  DISALLOW_COPY_AND_ASSIGN(UsbPrinterNotificationController);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_USB_PRINTER_NOTIFICATION_CONTROLLER_H_
