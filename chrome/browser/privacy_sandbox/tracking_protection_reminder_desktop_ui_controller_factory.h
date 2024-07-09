// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_TRACKING_PROTECTION_REMINDER_DESKTOP_UI_CONTROLLER_FACTORY_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_TRACKING_PROTECTION_REMINDER_DESKTOP_UI_CONTROLLER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/privacy_sandbox/tracking_protection_reminder_desktop_ui_controller.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

class TrackingProtectionReminderDesktopUiControllerFactory
    : public ProfileKeyedServiceFactory {
 public:
  static TrackingProtectionReminderDesktopUiControllerFactory* GetInstance();
  static privacy_sandbox::TrackingProtectionReminderDesktopUiController*
  GetForProfile(Profile* profile);

 private:
  friend base::NoDestructor<
      TrackingProtectionReminderDesktopUiControllerFactory>;
  TrackingProtectionReminderDesktopUiControllerFactory();
  ~TrackingProtectionReminderDesktopUiControllerFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;

  bool ServiceIsCreatedWithBrowserContext() const override;
};

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_TRACKING_PROTECTION_REMINDER_DESKTOP_UI_CONTROLLER_FACTORY_H_
