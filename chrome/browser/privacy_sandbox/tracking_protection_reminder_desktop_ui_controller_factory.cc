// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/tracking_protection_reminder_desktop_ui_controller_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/privacy_sandbox/tracking_protection_reminder_factory.h"
#include "chrome/browser/profiles/profile.h"

TrackingProtectionReminderDesktopUiControllerFactory*
TrackingProtectionReminderDesktopUiControllerFactory::GetInstance() {
  static base::NoDestructor<
      TrackingProtectionReminderDesktopUiControllerFactory>
      instance;
  return instance.get();
}

privacy_sandbox::TrackingProtectionReminderDesktopUiController*
TrackingProtectionReminderDesktopUiControllerFactory::GetForProfile(
    Profile* profile) {
  return static_cast<
      privacy_sandbox::TrackingProtectionReminderDesktopUiController*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

TrackingProtectionReminderDesktopUiControllerFactory::
    TrackingProtectionReminderDesktopUiControllerFactory()
    : ProfileKeyedServiceFactory(
          "TrackingProtectionReminderDesktopUiController",
          // Exclude Ash login and lockscreen.
          ProfileSelections::Builder()
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {
  DependsOn(TrackingProtectionReminderFactory::GetInstance());
}

std::unique_ptr<KeyedService>
TrackingProtectionReminderDesktopUiControllerFactory::
    BuildServiceInstanceForBrowserContext(
        content::BrowserContext* context) const {
  return std::make_unique<
      privacy_sandbox::TrackingProtectionReminderDesktopUiController>(
      TrackingProtectionReminderFactory::GetForProfile(
          Profile::FromBrowserContext(context)));
}

bool TrackingProtectionReminderDesktopUiControllerFactory::
    ServiceIsCreatedWithBrowserContext() const {
  return true;
}
