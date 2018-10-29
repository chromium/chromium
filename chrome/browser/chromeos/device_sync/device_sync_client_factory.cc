// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/device_sync/device_sync_client_factory.h"

#include "base/macros.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/pref_names.h"
#include "chromeos/chromeos_features.h"
#include "chromeos/services/device_sync/public/cpp/device_sync_client.h"
#include "chromeos/services/device_sync/public/cpp/device_sync_client_impl.h"
#include "chromeos/services/multidevice_setup/public/cpp/prefs.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"

namespace chromeos {

namespace device_sync {

namespace {

// CryptAuth enrollment is allowed only if at least one multi-device feature is
// enabled. This ensures that we do not unnecessarily register devices on the
// CryptAuth back-end when the registration would never actually be used.
bool IsEnrollmentAllowedByPolicy(content::BrowserContext* context) {
  return multidevice_setup::AreAnyMultiDeviceFeaturesAllowed(
      Profile::FromBrowserContext(context)->GetPrefs());
}

}  // namespace

// Class that wraps DeviceSyncClient in a KeyedService.
class DeviceSyncClientHolder : public KeyedService {
 public:
  explicit DeviceSyncClientHolder(content::BrowserContext* context)
      : device_sync_client_(DeviceSyncClientImpl::Factory::Get()->BuildInstance(
            content::BrowserContext::GetConnectorFor(context))) {}

  DeviceSyncClient* device_sync_client() { return device_sync_client_.get(); }

 private:
  // KeyedService:
  void Shutdown() override { device_sync_client_.reset(); }

  std::unique_ptr<DeviceSyncClient> device_sync_client_;

  DISALLOW_COPY_AND_ASSIGN(DeviceSyncClientHolder);
};

DeviceSyncClientFactory::DeviceSyncClientFactory()
    : BrowserContextKeyedServiceFactory(
          "DeviceSyncClient",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(gcm::GCMProfileServiceFactory::GetInstance());
}

DeviceSyncClientFactory::~DeviceSyncClientFactory() {}

// static
DeviceSyncClient* DeviceSyncClientFactory::GetForProfile(Profile* profile) {
  DeviceSyncClientHolder* holder = static_cast<DeviceSyncClientHolder*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));

  return holder ? holder->device_sync_client() : nullptr;
}

// static
DeviceSyncClientFactory* DeviceSyncClientFactory::GetInstance() {
  return base::Singleton<DeviceSyncClientFactory>::get();
}

KeyedService* DeviceSyncClientFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  // TODO(crbug.com/848347): Check prohibited by policy in services that depend
  // on this Factory, not here.
  if (IsEnrollmentAllowedByPolicy(context) &&
      base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi)) {
    return new DeviceSyncClientHolder(context);
  }

  return nullptr;
}

bool DeviceSyncClientFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace device_sync

}  // namespace chromeos
