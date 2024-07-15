// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/hid/hid_policy_allowed_devices_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/hid/hid_chooser_context_factory.h"
#include "chrome/browser/hid/hid_policy_allowed_devices.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#endif

// static
HidPolicyAllowedDevices* HidPolicyAllowedDevicesFactory::GetForProfile(
    Profile* profile) {
  return static_cast<HidPolicyAllowedDevices*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
HidPolicyAllowedDevicesFactory* HidPolicyAllowedDevicesFactory::GetInstance() {
  static base::NoDestructor<HidPolicyAllowedDevicesFactory> factory;
  return factory.get();
}

HidPolicyAllowedDevicesFactory::HidPolicyAllowedDevicesFactory()
    : ProfileKeyedServiceFactory(
          "HidPolicyAllowedDevices",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(HidChooserContextFactory::GetInstance());
}

std::unique_ptr<KeyedService>
HidPolicyAllowedDevicesFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  bool on_login_screen = false;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  on_login_screen = !ash::IsUserBrowserContext(context);
#endif
  return std::make_unique<HidPolicyAllowedDevices>(
      g_browser_process->local_state(), on_login_screen);
}
