// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/multidevice_setup/multidevice_setup_client_factory.h"

#include "base/macros.h"
#include "chrome/browser/chromeos/device_sync/device_sync_client_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/services/multidevice_setup/public/cpp/multidevice_setup_client.h"
#include "chromeos/services/multidevice_setup/public/cpp/multidevice_setup_client_impl.h"
#include "chromeos/services/multidevice_setup/public/cpp/prefs.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"

namespace chromeos {

namespace multidevice_setup {

namespace {

bool IsAllowedByPolicy(content::BrowserContext* context) {
  return multidevice_setup::AreAnyMultiDeviceFeaturesAllowed(
      Profile::FromBrowserContext(context)->GetPrefs());
}

}  // namespace

// Class that wraps MultiDeviceSetupClient in a KeyedService.
class MultiDeviceSetupClientHolder : public KeyedService {
 public:
  explicit MultiDeviceSetupClientHolder(content::BrowserContext* context)
      : multidevice_setup_client_(
            MultiDeviceSetupClientImpl::Factory::Get()->BuildInstance(
                content::BrowserContext::GetConnectorFor(context))) {}

  MultiDeviceSetupClient* multidevice_setup_client() {
    return multidevice_setup_client_.get();
  }

 private:
  // KeyedService:
  void Shutdown() override { multidevice_setup_client_.reset(); }

  std::unique_ptr<MultiDeviceSetupClient> multidevice_setup_client_;

  DISALLOW_COPY_AND_ASSIGN(MultiDeviceSetupClientHolder);
};

MultiDeviceSetupClientFactory::MultiDeviceSetupClientFactory()
    : BrowserContextKeyedServiceFactory(
          "MultiDeviceSetupClient",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(device_sync::DeviceSyncClientFactory::GetInstance());
}

MultiDeviceSetupClientFactory::~MultiDeviceSetupClientFactory() = default;

// static
MultiDeviceSetupClient* MultiDeviceSetupClientFactory::GetForProfile(
    Profile* profile) {
  if (!profile)
    return nullptr;

  MultiDeviceSetupClientHolder* holder =
      static_cast<MultiDeviceSetupClientHolder*>(
          GetInstance()->GetServiceForBrowserContext(profile, true));

  return holder ? holder->multidevice_setup_client() : nullptr;
}

// static
MultiDeviceSetupClientFactory* MultiDeviceSetupClientFactory::GetInstance() {
  return base::Singleton<MultiDeviceSetupClientFactory>::get();
}

KeyedService* MultiDeviceSetupClientFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  if (IsAllowedByPolicy(context))
    return new MultiDeviceSetupClientHolder(context);

  return nullptr;
}

bool MultiDeviceSetupClientFactory::ServiceIsNULLWhileTesting() const {
  return service_is_null_while_testing_;
}

}  // namespace multidevice_setup

}  // namespace chromeos
