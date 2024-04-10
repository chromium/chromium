// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"

#include "base/path_service.h"
#include "chrome/browser/ash/ownership/fake_owner_settings_service.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/dbus/constants/dbus_paths.h"
#include "components/ownership/owner_key_util.h"
#include "components/ownership/owner_key_util_impl.h"

namespace ash {

namespace {

DeviceSettingsService* g_device_settings_service_for_testing_ = nullptr;

StubCrosSettingsProvider* g_stub_cros_settings_provider_for_testing_ = nullptr;

DeviceSettingsService* GetDeviceSettingsService() {
  if (g_device_settings_service_for_testing_)
    return g_device_settings_service_for_testing_;
  return DeviceSettingsService::IsInitialized() ? DeviceSettingsService::Get()
                                                : nullptr;
}

}  // namespace

OwnerSettingsServiceAshFactory::OwnerSettingsServiceAshFactory()
    : ProfileKeyedServiceFactory("OwnerSettingsService",
                                 ProfileSelections::Builder()
                                     .WithGuest(ProfileSelection::kOriginalOnly)
                                     .WithAshInternals(ProfileSelection::kNone)
                                     .Build()) {}

OwnerSettingsServiceAshFactory::~OwnerSettingsServiceAshFactory() = default;

// static
OwnerSettingsServiceAsh* OwnerSettingsServiceAshFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<OwnerSettingsServiceAsh*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
OwnerSettingsServiceAshFactory* OwnerSettingsServiceAshFactory::GetInstance() {
  static base::NoDestructor<OwnerSettingsServiceAshFactory> instance;
  return instance.get();
}

// static
void OwnerSettingsServiceAshFactory::SetDeviceSettingsServiceForTesting(
    DeviceSettingsService* device_settings_service) {
  g_device_settings_service_for_testing_ = device_settings_service;
}

// static
void OwnerSettingsServiceAshFactory::SetStubCrosSettingsProviderForTesting(
    StubCrosSettingsProvider* stub_cros_settings_provider) {
  g_stub_cros_settings_provider_for_testing_ = stub_cros_settings_provider;
}

scoped_refptr<ownership::OwnerKeyUtil>
OwnerSettingsServiceAshFactory::GetOwnerKeyUtil() {
  if (owner_key_util_.get())
    return owner_key_util_;
  base::FilePath public_key_path;
  if (!base::PathService::Get(chromeos::dbus_paths::FILE_OWNER_KEY,
                              &public_key_path)) {
    return nullptr;
  }
  owner_key_util_ = new ownership::OwnerKeyUtilImpl(public_key_path);
  return owner_key_util_;
}

void OwnerSettingsServiceAshFactory::SetOwnerKeyUtilForTesting(
    const scoped_refptr<ownership::OwnerKeyUtil>& owner_key_util) {
  owner_key_util_ = owner_key_util;
}

bool OwnerSettingsServiceAshFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

KeyedService* OwnerSettingsServiceAshFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  // If g_stub_cros_settings_provider_for_testing_ is set, we treat the current
  // user as the owner, and write settings directly to the stubbed provider.
  // This is done using the FakeOwnerSettingsService.
  if (g_stub_cros_settings_provider_for_testing_ != nullptr) {
    return new FakeOwnerSettingsService(
        g_stub_cros_settings_provider_for_testing_, profile,
        GetInstance()->GetOwnerKeyUtil());
  }

  return new OwnerSettingsServiceAsh(GetDeviceSettingsService(), profile,
                                     GetInstance()->GetOwnerKeyUtil());
}

}  // namespace ash
