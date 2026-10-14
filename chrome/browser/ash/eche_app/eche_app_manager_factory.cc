// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/eche_app/eche_app_manager_factory.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/system/eche/eche_tray.h"
#include "ash/webui/eche_app_ui/apps_access_manager_impl.h"
#include "ash/webui/eche_app_ui/eche_app_manager.h"
#include "ash/webui/eche_app_ui/eche_uid_provider.h"
#include "chrome/browser/ash/device_sync/device_sync_client_factory.h"
#include "chrome/browser/ash/eche_app/eche_app_accessibility_provider_proxy.h"
#include "chrome/browser/ash/multidevice_setup/multidevice_setup_client_factory.h"
#include "chrome/browser/ash/phonehub/phone_hub_manager_factory.h"
#include "chrome/browser/ash/secure_channel/nearby_connector_factory.h"
#include "chrome/browser/ash/secure_channel/secure_channel_client_provider.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/phonehub/phone_hub_manager.h"
#include "chromeos/ash/services/secure_channel/presence_monitor_impl.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/presence_monitor_client_impl.h"
#include "chromeos/ash/services/secure_channel/public/cpp/shared/presence_monitor.h"
#include "components/pref_registry/pref_registry_syncable.h"

namespace ash {
namespace eche_app {

// static
EcheAppManager* EcheAppManagerFactory::GetForProfile(Profile* profile) {
  return static_cast<EcheAppManager*>(
      EcheAppManagerFactory::GetInstance()->GetServiceForBrowserContext(
          profile, /*create=*/true));
}

// static
EcheAppManagerFactory* EcheAppManagerFactory::GetInstance() {
  static base::NoDestructor<EcheAppManagerFactory> instance;
  return instance.get();
}

EcheAppManagerFactory::EcheAppManagerFactory()
    : ProfileKeyedServiceFactory(
          "EcheAppManager",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(phonehub::PhoneHubManagerFactory::GetInstance());
  DependsOn(device_sync::DeviceSyncClientFactory::GetInstance());
  DependsOn(multidevice_setup::MultiDeviceSetupClientFactory::GetInstance());
  DependsOn(secure_channel::NearbyConnectorFactory::GetInstance());
}

EcheAppManagerFactory::~EcheAppManagerFactory() = default;

void EcheAppManagerFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterStringPref(kEcheAppSeedPref, "");
  AppsAccessManagerImpl::RegisterPrefs(registry);
}

std::unique_ptr<KeyedService>
EcheAppManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!features::IsPhoneHubEnabled() || !features::IsEcheSWAEnabled())
    return nullptr;

  Profile* profile = Profile::FromBrowserContext(context);
  phonehub::PhoneHubManager* phone_hub_manager =
      phonehub::PhoneHubManagerFactory::GetForProfile(profile);
  if (!phone_hub_manager)
    return nullptr;

  device_sync::DeviceSyncClient* device_sync_client =
      device_sync::DeviceSyncClientFactory::GetForProfile(profile);
  if (!device_sync_client)
    return nullptr;

  multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client =
      multidevice_setup::MultiDeviceSetupClientFactory::GetForProfile(profile);
  if (!multidevice_setup_client)
    return nullptr;

  secure_channel::SecureChannelClient* secure_channel_client =
      secure_channel::SecureChannelClientProvider::GetInstance()->GetClient();
  if (!secure_channel_client)
    return nullptr;

  auto presence_monitor =
      std::make_unique<secure_channel::PresenceMonitorImpl>();
  std::unique_ptr<secure_channel::PresenceMonitorClient>
      presence_monitor_client =
          secure_channel::PresenceMonitorClientImpl::Factory::Create(
              std::move(presence_monitor));

  std::unique_ptr<EcheAppManager> eche_app_manager =
      std::make_unique<EcheAppManager>(
          profile->GetPrefs(),
          BrowserContextHelper::Get()->GetUserByBrowserContext(profile),
          phone_hub_manager, device_sync_client, multidevice_setup_client,
          secure_channel_client, std::move(presence_monitor_client),
          std::make_unique<EcheAppAccessibilityProviderProxy>());

  EcheTray* eche_tray = Shell::GetPrimaryRootWindowController()
                            ->GetStatusAreaWidget()
                            ->eche_tray();

  if (eche_tray) {
    eche_tray->SetEcheConnectionStatusHandler(
        eche_app_manager->GetEcheConnectionStatusHandler());
  }

  return eche_app_manager;
}

}  // namespace eche_app
}  // namespace ash
