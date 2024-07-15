// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/smart_card/fake_smart_card_device_service_factory.h"
#include "base/check_deref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/smart_card/fake_smart_card_device_service.h"

// static
FakeSmartCardDeviceServiceFactory&
FakeSmartCardDeviceServiceFactory::GetInstance() {
  static base::NoDestructor<FakeSmartCardDeviceServiceFactory> factory;
  return *factory;
}

// static
FakeSmartCardDeviceService& FakeSmartCardDeviceServiceFactory::GetForProfile(
    Profile& profile) {
  auto* service = static_cast<FakeSmartCardDeviceService*>(
      GetInstance().GetServiceForBrowserContext(&profile, /*create=*/true));
  return CHECK_DEREF(service);
}

FakeSmartCardDeviceServiceFactory::FakeSmartCardDeviceServiceFactory()
    : ProfileKeyedServiceFactory(
          "FakeSmartCardDeviceService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {}

FakeSmartCardDeviceServiceFactory::~FakeSmartCardDeviceServiceFactory() =
    default;

std::unique_ptr<KeyedService>
FakeSmartCardDeviceServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<FakeSmartCardDeviceService>();
}
