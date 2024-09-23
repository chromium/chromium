// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/phonehub/phone_hub_manager_factory.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/system_tray.h"
#include "chrome/browser/ash/attestation/soft_bind_attestation_flow_impl.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/device_sync/device_sync_client_factory.h"
#include "chrome/browser/ash/multidevice_setup/multidevice_setup_client_factory.h"
#include "chrome/browser/ash/phonehub/attestation_certificate_generator_impl.h"
#include "chrome/browser/ash/phonehub/browser_tabs_metadata_fetcher_impl.h"
#include "chrome/browser/ash/phonehub/browser_tabs_model_provider_impl.h"
#include "chrome/browser/ash/phonehub/camera_roll_download_manager_impl.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/secure_channel/nearby_connector_factory.h"
#include "chrome/browser/ash/secure_channel/secure_channel_client_provider.h"
#include "chrome/browser/ash/sync/sync_mojo_service_ash.h"
#include "chrome/browser/ash/sync/sync_mojo_service_factory_ash.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/favicon/history_ui_favicon_request_handler_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service_factory.h"
#include "chrome/browser/ui/webui/ash/multidevice_setup/multidevice_setup_dialog.h"
#include "chromeos/ash/components/phonehub/camera_roll_manager_impl.h"
#include "chromeos/ash/components/phonehub/multidevice_feature_access_manager_impl.h"
#include "chromeos/ash/components/phonehub/multidevice_setup_state_updater.h"
#include "chromeos/ash/components/phonehub/onboarding_ui_tracker_impl.h"
#include "chromeos/ash/components/phonehub/phone_hub_manager.h"
#include "chromeos/ash/components/phonehub/phone_hub_manager_impl.h"
#include "chromeos/ash/components/phonehub/phone_hub_structured_metrics_logger.h"
#include "chromeos/ash/components/phonehub/recent_apps_interaction_handler_impl.h"
#include "chromeos/ash/components/phonehub/screen_lock_manager_impl.h"
#include "chromeos/ash/components/phonehub/user_action_recorder_impl.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/prefs.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/sync/base/features.h"
#include "components/user_manager/user_manager.h"

namespace ash::phonehub {

namespace {

content::BrowserContext* g_context_for_service = nullptr;

bool IsProhibitedByPolicy(Profile* profile) {
  return !multidevice_setup::IsFeatureAllowed(
      multidevice_setup::mojom::Feature::kPhoneHub, profile->GetPrefs());
}

bool IsLoggedInAsPrimaryUser(Profile* profile) {
  // Guest/incognito/signin profiles cannot use Phone Hub.
  if (ash::ProfileHelper::IsSigninProfile(profile) ||
      profile->IsOffTheRecord()) {
    return false;
  }

  // Likewise, kiosk users are ineligible.
  if (user_manager::UserManager::Get()->IsLoggedInAsAnyKioskApp()) {
    return false;
  }

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
  static base::NoDestructor<PhoneHubManagerFactory> instance;
  return instance.get();
}

PhoneHubManagerFactory::PhoneHubManagerFactory()
    : ProfileKeyedServiceFactory(
          "PhoneHubManager",
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
  if (features::IsPhoneHubCameraRollEnabled()) {
    DependsOn(HoldingSpaceKeyedServiceFactory::GetInstance());
  }
  DependsOn(multidevice_setup::MultiDeviceSetupClientFactory::GetInstance());
  DependsOn(secure_channel::NearbyConnectorFactory::GetInstance());
  DependsOn(SessionSyncServiceFactory::GetInstance());
  DependsOn(HistoryUiFaviconRequestHandlerFactory::GetInstance());
  DependsOn(SyncServiceFactory::GetInstance());

  // We typically also check crosapi::browser_util::IsAshWebBrowserEnabled() in
  // relation to this feature flag but this relies on UserManager which is not
  // initialized at this point. Since this is just a service dependency simply
  // checking the flag itself is fine.
  if (base::FeatureList::IsEnabled(syncer::kChromeOSSyncedSessionSharing)) {
    DependsOn(SyncMojoServiceFactoryAsh::GetInstance());
  }
}

PhoneHubManagerFactory::~PhoneHubManagerFactory() = default;

std::unique_ptr<KeyedService>
PhoneHubManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!features::IsPhoneHubEnabled()) {
    return nullptr;
  }

  Profile* profile = Profile::FromBrowserContext(context);

  // Only available to the primary profile.
  if (!IsLoggedInAsPrimaryUser(profile)) {
    return nullptr;
  }

  if (IsProhibitedByPolicy(profile)) {
    return nullptr;
  }

  if (!features::IsCrossDeviceFeatureSuiteAllowed()) {
    return nullptr;
  }

  std::unique_ptr<AttestationCertificateGeneratorImpl>
      attestation_certificate_generator = nullptr;

  if (features::IsEcheSWAEnabled()) {
    auto soft_bind_attestation_flow =
        std::make_unique<attestation::SoftBindAttestationFlowImpl>();

    attestation_certificate_generator =
        std::make_unique<AttestationCertificateGeneratorImpl>(
            profile, std::move(soft_bind_attestation_flow));
  }

  auto phone_hub_manager = std::make_unique<PhoneHubManagerImpl>(
      profile->GetPrefs(),
      device_sync::DeviceSyncClientFactory::GetForProfile(profile),
      multidevice_setup::MultiDeviceSetupClientFactory::GetForProfile(profile),
      secure_channel::SecureChannelClientProvider::GetInstance()->GetClient(),
      std::make_unique<BrowserTabsModelProviderImpl>(
          multidevice_setup::MultiDeviceSetupClientFactory::GetForProfile(
              profile),
          SyncServiceFactory::GetInstance()->GetForProfile(profile),
          SessionSyncServiceFactory::GetInstance()->GetForProfile(profile),
          std::make_unique<BrowserTabsMetadataFetcherImpl>(
              HistoryUiFaviconRequestHandlerFactory::GetInstance()
                  ->GetForBrowserContext(context))),
      features::IsPhoneHubCameraRollEnabled()
          ? std::make_unique<CameraRollDownloadManagerImpl>(
                DownloadPrefs::FromDownloadManager(
                    profile->GetDownloadManager())
                    ->DownloadPath(),
                ash::HoldingSpaceKeyedServiceFactory::GetInstance()->GetService(
                    profile))
          : nullptr,
      base::BindRepeating(&multidevice_setup::MultiDeviceSetupDialog::Show),
      std::move(attestation_certificate_generator));

  // Provide |phone_hub_manager| to the system tray so that it can be used by
  // the UI.
  SystemTray::Get()->SetPhoneHubManager(phone_hub_manager.get());

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
  // causes SyncService to be created before SyncConsentScreen. Instead,
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
    auto* system_tray = SystemTray::Get();
    if (system_tray) {
      system_tray->SetPhoneHubManager(nullptr);
    }

    g_context_for_service = nullptr;
  }

  BrowserContextKeyedServiceFactory::BrowserContextShutdown(context);
}

void PhoneHubManagerFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  MultideviceSetupStateUpdater::RegisterPrefs(registry);
  MultideviceFeatureAccessManagerImpl::RegisterPrefs(registry);
  OnboardingUiTrackerImpl::RegisterPrefs(registry);
  ScreenLockManagerImpl::RegisterPrefs(registry);
  RecentAppsInteractionHandlerImpl::RegisterPrefs(registry);
  PhoneHubStructuredMetricsLogger::RegisterPrefs(registry);
}

}  // namespace ash::phonehub
