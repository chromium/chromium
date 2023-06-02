// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/tether/tether_service_factory.h"

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ash/device_sync/device_sync_client_factory.h"
#include "chrome/browser/ash/multidevice_setup/multidevice_setup_client_factory.h"
#include "chrome/browser/ash/secure_channel/secure_channel_client_provider.h"
#include "chrome/browser/ash/tether/fake_tether_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/prefs.h"
#include "chromeos/ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/session_manager/core/session_manager.h"

namespace ash {
namespace tether {

namespace {

bool IsFeatureAllowed(content::BrowserContext* context) {
  return multidevice_setup::IsFeatureAllowed(
      multidevice_setup::mojom::Feature::kInstantTethering,
      Profile::FromBrowserContext(context)->GetPrefs());
}

}  // namespace

// static
TetherServiceFactory* TetherServiceFactory::GetInstance() {
  static base::NoDestructor<TetherServiceFactory> instance;
  return instance.get();
}

// static
TetherService* TetherServiceFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<TetherService*>(
      TetherServiceFactory::GetInstance()->GetServiceForBrowserContext(
          browser_context, true));
}

TetherServiceFactory::TetherServiceFactory()
    : ProfileKeyedServiceFactory(
          "TetherService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(device_sync::DeviceSyncClientFactory::GetInstance());
  DependsOn(multidevice_setup::MultiDeviceSetupClientFactory::GetInstance());
}

TetherServiceFactory::~TetherServiceFactory() = default;

KeyedService* TetherServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  DCHECK(NetworkHandler::IsInitialized());

  if (!IsFeatureAllowed(context))
    return nullptr;

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kTetherStub)) {
    FakeTetherService* fake_tether_service = new FakeTetherService(
        Profile::FromBrowserContext(context),
        chromeos::PowerManagerClient::Get(),
        device_sync::DeviceSyncClientFactory::GetForProfile(
            Profile::FromBrowserContext(context)),
        secure_channel::SecureChannelClientProvider::GetInstance()->GetClient(),
        multidevice_setup::MultiDeviceSetupClientFactory::GetForProfile(
            Profile::FromBrowserContext(context)),
        NetworkHandler::Get()->network_state_handler(),
        session_manager::SessionManager::Get());

    int num_tether_networks = 0;
    base::StringToInt(command_line->GetSwitchValueASCII(switches::kTetherStub),
                      &num_tether_networks);
    fake_tether_service->set_num_tether_networks(num_tether_networks);

    return fake_tether_service;
  }

  return new TetherService(
      Profile::FromBrowserContext(context), chromeos::PowerManagerClient::Get(),
      device_sync::DeviceSyncClientFactory::GetForProfile(
          Profile::FromBrowserContext(context)),
      secure_channel::SecureChannelClientProvider::GetInstance()->GetClient(),
      multidevice_setup::MultiDeviceSetupClientFactory::GetForProfile(
          Profile::FromBrowserContext(context)),
      NetworkHandler::Get()->network_state_handler(),
      session_manager::SessionManager::Get());
}

void TetherServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  TetherService::RegisterProfilePrefs(registry);
}

bool TetherServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace tether
}  // namespace ash
