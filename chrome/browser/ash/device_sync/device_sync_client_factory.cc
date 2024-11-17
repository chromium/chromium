// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/device_sync/device_sync_client_factory.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/single_thread_task_runner.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/attestation/soft_bind_attestation_flow_impl.h"
#include "chrome/browser/ash/cryptauth/client_app_metadata_provider_service.h"
#include "chrome/browser/ash/cryptauth/client_app_metadata_provider_service_factory.h"
#include "chrome/browser/ash/cryptauth/gcm_device_info_provider_impl.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/gcm/instance_id/instance_id_profile_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chromeos/ash/components/multidevice/stub_multidevice_util.h"
#include "chromeos/ash/services/device_sync/device_sync_impl.h"
#include "chromeos/ash/services/device_sync/public/cpp/device_sync_client.h"
#include "chromeos/ash/services/device_sync/public/cpp/device_sync_client_impl.h"
#include "chromeos/ash/services/device_sync/stub_device_sync.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/prefs.h"
#include "components/account_id/account_id.h"
#include "components/gcm_driver/gcm_profile_service.h"
#include "components/gcm_driver/instance_id/instance_id_profile_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ash {
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
      : soft_bind_attestation_flow_(
            std::make_unique<attestation::SoftBindAttestationFlowImpl>()),
        device_sync_(CreateDeviceSyncImplForProfile(
            Profile::FromBrowserContext(context))),
        device_sync_client_(DeviceSyncClientImpl::Factory::Create()) {
    // Connect the client's mojo remote to the implementation.
    device_sync_->BindReceiver(device_sync_client_->GetDeviceSyncRemote()
                                   ->BindNewPipeAndPassReceiver());
    // Finish client initialization.
    device_sync_client_->Initialize(
        base::SingleThreadTaskRunner::GetCurrentDefault());
  }

  DeviceSyncClientHolder(const DeviceSyncClientHolder&) = delete;
  DeviceSyncClientHolder& operator=(const DeviceSyncClientHolder&) = delete;

  DeviceSyncClient* device_sync_client() { return device_sync_client_.get(); }

 private:
  // KeyedService:
  void Shutdown() override {
    device_sync_client_.reset();
    device_sync_->CloseAllReceivers();
    device_sync_.reset();
    soft_bind_attestation_flow_.reset();
  }

  std::unique_ptr<DeviceSyncBase> CreateDeviceSyncImplForProfile(
      Profile* profile) {
    return DeviceSyncImpl::Factory::Create(
        IdentityManagerFactory::GetForProfile(profile),
        gcm::GCMProfileServiceFactory::GetForProfile(profile)->driver(),
        instance_id::InstanceIDProfileServiceFactory::GetForProfile(profile)
            ->driver(),
        profile->GetPrefs(), GcmDeviceInfoProviderImpl::GetInstance(),
        ClientAppMetadataProviderServiceFactory::GetForProfile(profile),
        profile->GetURLLoaderFactory(), std::make_unique<base::OneShotTimer>(),
        base::BindRepeating(&DeviceSyncClientHolder::GetAttestationCertificates,
                            base::Unretained(this), profile));
  }

  void GetAttestationCertificates(
      Profile* profile,
      attestation::SoftBindAttestationFlow::Callback notify_callback,
      const std::string& user_key) {
    const user_manager::User* user =
        ProfileHelper::Get()->GetUserByProfile(profile);
    soft_bind_attestation_flow_->GetCertificate(
        std::move(notify_callback),
        user ? user->GetAccountId() : EmptyAccountId(), user_key);
  }

  std::unique_ptr<attestation::SoftBindAttestationFlowImpl>
      soft_bind_attestation_flow_;

  std::unique_ptr<DeviceSyncBase> device_sync_;
  std::unique_ptr<DeviceSyncClient> device_sync_client_;
};

DeviceSyncClientFactory::DeviceSyncClientFactory()
    : ProfileKeyedServiceFactory(
          "DeviceSyncClient",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(ClientAppMetadataProviderServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(instance_id::InstanceIDProfileServiceFactory::GetInstance());

  // If ShouldUseMultideviceStubs() is true, set a stub factory to facilitate
  // fake devices for testing in the Linux Chrome OS build. Note that this is
  // not done when a custom factory has already been set.
  if (multidevice::ShouldUseMultideviceStubs() &&
      !DeviceSyncImpl::Factory::IsCustomFactorySet()) {
    SetStubDeviceSyncFactory();
  }
}

DeviceSyncClientFactory::~DeviceSyncClientFactory() = default;

// static
DeviceSyncClient* DeviceSyncClientFactory::GetForProfile(Profile* profile) {
  DeviceSyncClientHolder* holder = static_cast<DeviceSyncClientHolder*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));

  return holder ? holder->device_sync_client() : nullptr;
}

// static
DeviceSyncClientFactory* DeviceSyncClientFactory::GetInstance() {
  static base::NoDestructor<DeviceSyncClientFactory> instance;
  return instance.get();
}

std::unique_ptr<KeyedService>
DeviceSyncClientFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  // TODO(crbug.com/848347): Check prohibited by policy in services that depend
  // on this Factory, not here.
  if (IsEnrollmentAllowedByPolicy(context))
    return std::make_unique<DeviceSyncClientHolder>(context);

  return nullptr;
}

bool DeviceSyncClientFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace device_sync
}  // namespace ash
