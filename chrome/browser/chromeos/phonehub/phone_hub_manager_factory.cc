// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/phonehub/phone_hub_manager_factory.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/system_tray.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/chromeos/device_sync/device_sync_client_factory.h"
#include "chrome/browser/chromeos/multidevice_setup/multidevice_setup_client_factory.h"
#include "chrome/browser/chromeos/phonehub/browser_tabs_metadata_fetcher_impl.h"
#include "chrome/browser/chromeos/phonehub/browser_tabs_model_provider_impl.h"
#include "chrome/browser/chromeos/secure_channel/nearby_connector_factory.h"
#include "chrome/browser/chromeos/secure_channel/secure_channel_client_provider.h"
#include "chrome/browser/favicon/history_ui_favicon_request_handler_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/browser/ui/webui/chromeos/multidevice_setup/multidevice_setup_dialog.h"
#include "chromeos/components/phonehub/multidevice_setup_state_updater.h"
#include "chromeos/components/phonehub/notification_access_manager_impl.h"
#include "chromeos/components/phonehub/onboarding_ui_tracker_impl.h"
#include "chromeos/components/phonehub/phone_hub_manager_impl.h"
#include "chromeos/components/phonehub/user_action_recorder_impl.h"
#include "chromeos/services/multidevice_setup/public/cpp/prefs.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"

namespace chromeos {
namespace phonehub {
namespace {

content::BrowserContext* g_context_for_service = nullptr;

bool IsProhibitedByPolicy(Profile* profile) {
  return !multidevice_setup::IsFeatureAllowed(
      multidevice_setup::mojom::Feature::kPhoneHub, profile->GetPrefs());
}

bool IsLoggedInAsPrimaryUser(Profile* profile) {
  // Guest/incognito profiles cannot use Phone Hub.
  if (profile->IsOffTheRecord())
    return false;

  // Likewise, kiosk users are ineligible.
  if (user_manager::UserManager::Get()->IsLoggedInAsAnyKioskApp())
    return false;

  return ProfileHelper::IsPrimaryProfile(profile);
}

}  // namespace

// static
PhoneHubManager* PhoneHubManagerFactory::GetForProfile(Profile* profile) {
  return static_cast<PhoneHubManagerImpl*>(
      PhoneHubManagerFactory::GetInstance()->GetServiceForBrowserContext(
          profile, /*create=*/true));
}

// static
PhoneHubManagerFactory* PhoneHubManagerFactory::GetInstance() {
  return base::Singleton<PhoneHubManagerFactory>::get();
}

PhoneHubManagerFactory::PhoneHubManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "PhoneHubManager",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(device_sync::DeviceSyncClientFactory::GetInstance());
  DependsOn(multidevice_setup::MultiDeviceSetupClientFactory::GetInstance());
  DependsOn(secure_channel::NearbyConnectorFactory::GetInstance());
  DependsOn(SessionSyncServiceFactory::GetInstance());
  DependsOn(HistoryUiFaviconRequestHandlerFactory::GetInstance());
  DependsOn(ProfileSyncServiceFactory::GetInstance());
}

PhoneHubManagerFactory::~PhoneHubManagerFactory() = default;

KeyedService* PhoneHubManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  if (!features::IsPhoneHubEnabled())
    return nullptr;

  Profile* profile = Profile::FromBrowserContext(context);

  // Only available to the primary profile.
  if (!IsLoggedInAsPrimaryUser(profile))
    return nullptr;

  if (IsProhibitedByPolicy(profile))
    return nullptr;

  PhoneHubManagerImpl* phone_hub_manager = new PhoneHubManagerImpl(
      profile->GetPrefs(),
      device_sync::DeviceSyncClientFactory::GetForProfile(profile),
      multidevice_setup::MultiDeviceSetupClientFactory::GetForProfile(profile),
      secure_channel::SecureChannelClientProvider::GetInstance()->GetClient(),
      std::make_unique<BrowserTabsModelProviderImpl>(
          multidevice_setup::MultiDeviceSetupClientFactory::GetForProfile(
              profile),
          ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile),
          SessionSyncServiceFactory::GetInstance()->GetForProfile(profile),
          std::make_unique<BrowserTabsMetadataFetcherImpl>(
              HistoryUiFaviconRequestHandlerFactory::GetInstance()
                  ->GetForBrowserContext(context))),
      base::BindRepeating(&multidevice_setup::MultiDeviceSetupDialog::Show));

  // Provide |phone_hub_manager| to the system tray so that it can be used by
  // the UI.
  ash::SystemTray::Get()->SetPhoneHubManager(phone_hub_manager);

  DCHECK(!g_context_for_service);
  g_context_for_service = context;

  return phone_hub_manager;
}

bool PhoneHubManagerFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

bool PhoneHubManagerFactory::ServiceIsCreatedWithBrowserContext() const {
  // We do want the service to be created with the BrowserContext, but returning
  // true here causes issues when opting into Chrome Sync in OOBE because it
  // causes ProfileSyncService to be created before SyncConsentScreen. Instead,
  // we return false here and initialize PhoneHubManager within
  // UserSessionInitializer.
  return false;
}

void PhoneHubManagerFactory::BrowserContextShutdown(
    content::BrowserContext* context) {
  // If the primary Profile is being deleted, notify SystemTray that
  // PhoneHubManager is being deleted. Note that the SystemTray is normally
  // expected to be deleted *before* the Profile, so this check is only expected
  // to be necessary in tests.
  if (g_context_for_service == context) {
    ash::SystemTray* system_tray = ash::SystemTray::Get();
    if (system_tray)
      system_tray->SetPhoneHubManager(nullptr);

    g_context_for_service = nullptr;
  }

  BrowserContextKeyedServiceFactory::BrowserContextShutdown(context);
}

void PhoneHubManagerFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  MultideviceSetupStateUpdater::RegisterPrefs(registry);
  NotificationAccessManagerImpl::RegisterPrefs(registry);
  OnboardingUiTrackerImpl::RegisterPrefs(registry);
}

}  // namespace phonehub
}  // namespace chromeos
