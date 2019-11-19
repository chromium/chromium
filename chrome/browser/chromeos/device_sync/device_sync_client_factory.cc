// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/device_sync/device_sync_client_factory.h"

#include "base/macros.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/cryptauth/client_app_metadata_provider_service.h"
#include "chrome/browser/chromeos/cryptauth/client_app_metadata_provider_service_factory.h"
#include "chrome/browser/chromeos/cryptauth/gcm_device_info_provider_impl.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chromeos/services/device_sync/device_sync_impl.h"
#include "chromeos/services/device_sync/public/cpp/device_sync_client.h"
#include "chromeos/services/device_sync/public/cpp/device_sync_client_impl.h"
#include "chromeos/services/multidevice_setup/public/cpp/prefs.h"
#include "components/gcm_driver/gcm_profile_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

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

std::unique_ptr<DeviceSyncBase> CreateDeviceSyncImplForProfile(
    Profile* profile) {
  return DeviceSyncImpl::Factory::Get()->BuildInstance(
      IdentityManagerFactory::GetForProfile(profile),
      gcm::GCMProfileServiceFactory::GetForProfile(profile)->driver(),
      profile->GetPrefs(), chromeos::GcmDeviceInfoProviderImpl::GetInstance(),
      chromeos::ClientAppMetadataProviderServiceFactory::GetForProfile(profile),
      profile->GetURLLoaderFactory(), std::make_unique<base::OneShotTimer>());
}

}  // namespace

// Class that wraps DeviceSyncClient in a KeyedService.
class DeviceSyncClientHolder : public KeyedService {
 public:
  explicit DeviceSyncClientHolder(content::BrowserContext* context)
      : device_sync_(CreateDeviceSyncImplForProfile(
            Profile::FromBrowserContext(context))),
        device_sync_client_(
            DeviceSyncClientImpl::Factory::Get()->BuildInstance()) {
    // Connect the client's mojo remote to the implementation.
    device_sync_->BindReceiver(device_sync_client_->GetDeviceSyncRemote()
                                   ->BindNewPipeAndPassReceiver());
    // Finish client initialization.
    device_sync_client_->Initialize(base::ThreadTaskRunnerHandle::Get());
  }

  DeviceSyncClient* device_sync_client() { return device_sync_client_.get(); }

 private:
  // KeyedService:
  void Shutdown() override {
    device_sync_client_.reset();
    device_sync_->CloseAllReceivers();
    device_sync_.reset();
  }

  std::unique_ptr<DeviceSyncBase> device_sync_;
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
  if (IsEnrollmentAllowedByPolicy(context))
    return new DeviceSyncClientHolder(context);

  return nullptr;
}

bool DeviceSyncClientFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace device_sync

}  // namespace chromeos
