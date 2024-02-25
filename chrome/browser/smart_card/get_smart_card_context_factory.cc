// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/smart_card/get_smart_card_context_factory.h"

#if BUILDFLAG(IS_CHROMEOS_DEVICE)
#include "chrome/browser/chromeos/extensions/smart_card_provider_private/smart_card_provider_private_api.h"
#else
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/smart_card/fake_smart_card_device_service.h"
#include "chrome/browser/smart_card/fake_smart_card_device_service_factory.h"
#endif  // BUILDFLAG(IS_CHROMEOS_DEVICE)

mojo::PendingRemote<device::mojom::SmartCardContextFactory>
GetSmartCardContextFactory(content::BrowserContext& browser_context) {
#if BUILDFLAG(IS_CHROMEOS_DEVICE)
  // Get the smart card (PC/SC) service from a provider extension.
  return extensions::SmartCardProviderPrivateAPI::Get(browser_context)
      .GetSmartCardContextFactory();
#else
  // Emulate the smart card (PC/SC) service when running a ChromeOS build on
  // Linux. This makes make manual testing of features like permission prompt
  // and system tray indicator possible in a developer's machine.
  return FakeSmartCardDeviceServiceFactory::GetForProfile(
             *Profile::FromBrowserContext(&browser_context))
      .GetSmartCardContextFactory();
#endif  // BUILDFLAG(IS_CHROMEOS_DEVICE)
}
