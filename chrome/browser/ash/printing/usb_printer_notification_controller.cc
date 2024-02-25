// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/usb_printer_notification_controller.h"

#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/profiles/profile.h"

namespace ash {

class UsbPrinterNotificationControllerImpl
    : public UsbPrinterNotificationController {
 public:
  explicit UsbPrinterNotificationControllerImpl(Profile* profile)
      : profile_(profile) {}
  ~UsbPrinterNotificationControllerImpl() override = default;

  void ShowEphemeralNotification(const chromeos::Printer& printer) override {
    ShowNotification(printer, UsbPrinterNotification::Type::kEphemeral);
  }

  void RemoveNotification(const std::string& printer_id) override {
    if (!base::Contains(notifications_, printer_id)) {
      return;
    }
    notifications_[printer_id]->CloseNotification();
    notifications_.erase(printer_id);
  }

  bool IsNotificationDisplayed(const std::string& printer_id) const override {
    return base::Contains(notifications_, printer_id);
  }

  void ShowSavedNotification(const chromeos::Printer& printer) override {
    ShowNotification(printer, UsbPrinterNotification::Type::kSaved);
  }

  void ShowConfigurationNotification(
      const chromeos::Printer& printer) override {
    ShowNotification(printer,
                     UsbPrinterNotification::Type::kConfigurationRequired);
  }

 private:
  void ShowNotification(const chromeos::Printer& printer,
                        UsbPrinterNotification::Type type) {
    if (base::Contains(notifications_, printer.id())) {
      return;
    }

    notifications_[printer.id()] = std::make_unique<UsbPrinterNotification>(
        printer, GetUniqueNotificationId(), type, profile_);
  }

  std::string GetUniqueNotificationId() {
    return base::StringPrintf("usb_printer_notification_%d",
                              next_notification_id_++);
  }

  std::map<std::string, std::unique_ptr<UsbPrinterNotification>> notifications_;
  raw_ptr<Profile> profile_;
  int next_notification_id_ = 0;
};

std::unique_ptr<UsbPrinterNotificationController>
UsbPrinterNotificationController::Create(Profile* profile) {
  // If we are in guest mode, the new profile should be an OffTheRecord profile.
  // Otherwise, this may later hit a check (same condition as this one) in
  // Browser::Browser when opening attempting to open the Printer Settings page.
  DCHECK(!profile->IsGuestSession() || profile->IsOffTheRecord())
      << "Guest mode must use OffTheRecord profile";
  return std::make_unique<UsbPrinterNotificationControllerImpl>(profile);
}

}  // namespace ash
