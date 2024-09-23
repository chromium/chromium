// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/multidevice_setup/multidevice_setup_client_factory.h"

#include "ash/constants/ash_features.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ash/device_sync/device_sync_client_factory.h"
#include "chrome/browser/ash/multidevice_setup/multidevice_setup_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/services/multidevice_setup/multidevice_setup_service.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/multidevice_setup_client.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/multidevice_setup_client_impl.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/prefs.h"
#include "chromeos/ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"

namespace ash {
namespace multidevice_setup {

namespace {

bool IsAllowedByPolicy(content::BrowserContext* context) {
  return multidevice_setup::AreAnyMultiDeviceFeaturesAllowed(
      Profile::FromBrowserContext(context)->GetPrefs());
}

// Class that wraps MultiDeviceSetupClient in a KeyedService.
class MultiDeviceSetupClientHolder : public KeyedService {
 public:
  explicit MultiDeviceSetupClientHolder(content::BrowserContext* context)
      : profile_(Profile::FromBrowserContext(context)) {
    mojo::PendingRemote<mojom::MultiDeviceSetup> remote_setup;
    auto receiver = remote_setup.InitWithNewPipeAndPassReceiver();
    multidevice_setup_client_ =
        MultiDeviceSetupClientImpl::Factory::Create(std::move(remote_setup));

    // NOTE: We bind the receiver asynchronously, because we can't synchronously
    // depend on MultiDeviceSetupServiceFactory at construction time. This is
    // due to a circular dependency among the AndroidSmsServiceFactory,
    // MultiDeviceSetupServiceFactory, and MultiDeviceSetupClientFactory
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&MultiDeviceSetupClientHolder::BindService,
                       weak_factory_.GetWeakPtr(), std::move(receiver)));
  }

  MultiDeviceSetupClientHolder(const MultiDeviceSetupClientHolder&) = delete;
  MultiDeviceSetupClientHolder& operator=(const MultiDeviceSetupClientHolder&) =
      delete;

  MultiDeviceSetupClient* multidevice_setup_client() {
    return multidevice_setup_client_.get();
  }

 private:
  void BindService(mojo::PendingReceiver<mojom::MultiDeviceSetup> receiver) {
    MultiDeviceSetupServiceFactory::GetForProfile(profile_)
        ->BindMultiDeviceSetup(std::move(receiver));
  }

  // KeyedService:
  void Shutdown() override {
    multidevice_setup_client_.reset();
    weak_factory_.InvalidateWeakPtrs();
  }

  const raw_ptr<Profile> profile_;
  std::unique_ptr<MultiDeviceSetupClient> multidevice_setup_client_;
  base::WeakPtrFactory<MultiDeviceSetupClientHolder> weak_factory_{this};
};

}  // namespace

MultiDeviceSetupClientFactory::MultiDeviceSetupClientFactory()
    : ProfileKeyedServiceFactory(
          "MultiDeviceSetupClient",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(device_sync::DeviceSyncClientFactory::GetInstance());
  // The MultiDeviceSetupServiceFactory dependency is omitted here, see the
  // comment in the MultiDeviceSetupClientHolder constructor.
}

MultiDeviceSetupClientFactory::~MultiDeviceSetupClientFactory() = default;

// static
MultiDeviceSetupClient* MultiDeviceSetupClientFactory::GetForProfile(
    Profile* profile) {
  if (!profile) {
    PA_LOG(WARNING) << "Missing Profile. Unable to return "
                       "MultiDeviceSetupClient, returning nullptr instead.";
    return nullptr;
  }

  MultiDeviceSetupClientHolder* holder =
      static_cast<MultiDeviceSetupClientHolder*>(
          MultiDeviceSetupClientFactory::GetInstance()
              ->GetServiceForBrowserContext(profile, true));

  if (!holder) {
    PA_LOG(WARNING) << "Missing MultiDeviceSetupClientHolder. Unable to return "
                       "MultiDeviceSetupClient, returning nullptr instead.";
    return nullptr;
  }

  return holder->multidevice_setup_client();
}

// static
MultiDeviceSetupClientFactory* MultiDeviceSetupClientFactory::GetInstance() {
  static base::NoDestructor<MultiDeviceSetupClientFactory> instance;
  return instance.get();
}

std::unique_ptr<KeyedService>
MultiDeviceSetupClientFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!IsAllowedByPolicy(context)) {
    PA_LOG(INFO) << "NOT allowed by policy. Unable to return "
                    "MultiDeviceSetupClientHolder, returning nullptr instead.";
    return nullptr;
  }

  if (!features::IsCrossDeviceFeatureSuiteAllowed()) {
    PA_LOG(INFO) << "Cross-device feature suite is disabled. Unable to return "
                    "MultiDeviceSetupClientHolder, returning nullptr instead.";
    return nullptr;
  }

  PA_LOG(INFO) << "Allowed. Returning new MultiDeviceSetupClientHolder";
  return std::make_unique<MultiDeviceSetupClientHolder>(context);
}

bool MultiDeviceSetupClientFactory::ServiceIsNULLWhileTesting() const {
  return service_is_null_while_testing_;
}

}  // namespace multidevice_setup
}  // namespace ash
