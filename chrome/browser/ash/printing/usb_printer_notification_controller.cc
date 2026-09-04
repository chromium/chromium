// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/usb_printer_notification_controller.h"

#include <map>
#include <memory>
#include <string>

#include "base/check_deref.h"
#include "base/memory/raw_ref.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ash/printing/usb_printer_notification.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "components/user_manager/user.h"

namespace ash {

class UsbPrinterNotificationControllerImpl
    : public UsbPrinterNotificationController {
 public:
  explicit UsbPrinterNotificationControllerImpl(const user_manager::User& user)
      : user_(user) {}
  ~UsbPrinterNotificationControllerImpl() override = default;

  void ShowEphemeralNotification(const chromeos::Printer& printer) override {
    ShowNotification(printer, UsbPrinterNotification::Type::kEphemeral);
  }

  void RemoveNotification(const std::string& printer_id) override {
    if (!notifications_.contains(printer_id)) {
      return;
    }
    notifications_[printer_id]->CloseNotification();
    notifications_.erase(printer_id);
  }

  bool IsNotificationDisplayed(const std::string& printer_id) const override {
    return notifications_.contains(printer_id);
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
    if (notifications_.contains(printer.id())) {
      return;
    }

    notifications_[printer.id()] = std::make_unique<UsbPrinterNotification>(
        printer, GetUniqueNotificationId(), type, *user_);
  }

  std::string GetUniqueNotificationId() {
    return base::StringPrintf("usb_printer_notification_%d",
                              next_notification_id_++);
  }

  std::map<std::string, std::unique_ptr<UsbPrinterNotification>> notifications_;
  const raw_ref<const user_manager::User> user_;
  int next_notification_id_ = 0;
};

std::unique_ptr<UsbPrinterNotificationController>
UsbPrinterNotificationController::Create(Profile* profile) {
  // If we are in guest mode, the new profile should be an OffTheRecord profile.
  // Otherwise, this may later hit a check (same condition as this one) in
  // Browser::Browser when opening attempting to open the Printer Settings page.
  DCHECK(!profile->IsGuestSession() || profile->IsOffTheRecord())
      << "Guest mode must use OffTheRecord profile";
  // Some browser tests create profiles without corresponding users and set
  // kIgnoreUserProfileMappingForTests. ProfileHelper honors that test-only
  // fallback; for regular profiles it delegates to BrowserContextHelper.
  return std::make_unique<UsbPrinterNotificationControllerImpl>(
      CHECK_DEREF(ProfileHelper::Get()->GetUserByProfile(profile)));
}

}  // namespace ash
