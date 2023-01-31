// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_service_factory.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/consent_auditor/consent_auditor_factory.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/power_bookmarks/power_bookmark_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/security_events/security_event_recorder_factory.h"
#include "chrome/browser/sharing/sharing_message_bridge_factory.h"
#include "chrome/browser/signin/about_signin_internals_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/sync/bookmark_sync_service_factory.h"
#include "chrome/browser/sync/chrome_sync_client.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/browser/sync/model_type_store_service_factory.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/browser/sync/sync_invalidations_service_factory.h"
#include "chrome/browser/sync/user_event_service_factory.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/undo/bookmark_undo_service_factory.h"
#include "chrome/browser/web_applications/web_app_provider_factory.h"
#include "chrome/browser/web_data_service_factory.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/channel_info.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/network_time/network_time_tracker.h"
#include "components/supervised_user/core/common/buildflags.h"
#include "components/sync/base/command_line_switches.h"
#include "components/sync/driver/sync_service_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/buildflags/buildflags.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/api/storage/storage_frontend.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/printing/oauth2/authorization_zones_manager_factory.h"
#include "chrome/browser/ash/printing/synced_printers_manager_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/sync/desk_sync_service_factory.h"
#include "chrome/browser/sync/wifi_configuration_sync_service_factory.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_WIN)
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_service_factory.h"
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) ||
        // BUILDFLAG(IS_WIN)

namespace {

std::unique_ptr<KeyedService> BuildSyncService(
    content::BrowserContext* context) {
  syncer::SyncServiceImpl::InitParams init_params;

  Profile* profile = Profile::FromBrowserContext(context);

  DCHECK(!profile->IsOffTheRecord());
  // Incognito, guest, or system profiles aren't relevant for Sync, and
  // eventually no SyncService should even be created for those types of
  // profiles. For now, they're just excluded from some startup metrics.
  init_params.is_regular_profile_for_uma = profile->IsRegularProfile();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // On Ash, there are additional non-interesting profile types (sign-in
  // profile and lockscreen profile).
  init_params.is_regular_profile_for_uma =
      init_params.is_regular_profile_for_uma &&
      ash::ProfileHelper::IsUserProfile(profile);
#endif

  init_params.sync_client =
      std::make_unique<browser_sync::ChromeSyncClient>(profile);
  init_params.url_loader_factory = profile->GetDefaultStoragePartition()
                                       ->GetURLLoaderFactoryForBrowserProcess();
  init_params.network_connection_tracker =
      content::GetNetworkConnectionTracker();
  init_params.channel = chrome::GetChannel();
  init_params.debug_identifier = profile->GetDebugName();

  bool local_sync_backend_enabled = false;
// Only check the local sync backend pref on the supported platforms of
// Windows, Mac and Linux.
// TODO(crbug.com/1052397): Reassess whether the following block needs to be
// included in lacros-chrome once build flag switch of lacros-chrome is
// complete.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
  syncer::SyncPrefs prefs(profile->GetPrefs());
  local_sync_backend_enabled = prefs.IsLocalSyncEnabled();
  if (init_params.is_regular_profile_for_uma) {
    base::UmaHistogramBoolean("Sync.Local.Enabled2",
                              local_sync_backend_enabled);
  }

  if (local_sync_backend_enabled) {
    base::FilePath local_sync_backend_folder =
        init_params.sync_client->GetLocalSyncBackendFolder();

    // If the user has not specified a folder and we can't get the default
    // roaming profile location the sync service will not be created.
    if (init_params.is_regular_profile_for_uma) {
      base::UmaHistogramBoolean("Sync.Local.RoamingProfileUnavailable2",
                                local_sync_backend_folder.empty());
    }
    if (local_sync_backend_folder.empty()) {
      return nullptr;
    }

    init_params.start_behavior = syncer::SyncServiceImpl::AUTO_START;
  }
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || (BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS_LACROS))

  if (!local_sync_backend_enabled) {
    // Always create the GCMProfileService instance such that we can listen to
    // the profile notifications and purge the GCM store when the profile is
    // being signed out.
    gcm::GCMProfileServiceFactory::GetForProfile(profile);

    // TODO(atwilson): Change AboutSigninInternalsFactory to load on startup
    // once http://crbug.com/171406 has been fixed.
    AboutSigninInternalsFactory::GetForProfile(profile);

    init_params.identity_manager =
        IdentityManagerFactory::GetForProfile(profile);

    // TODO(tim): Currently, AUTO/MANUAL settings refer to the *first* time sync
    // is set up and *not* a browser restart for a manual-start platform (where
    // sync has already been set up, and should be able to start without user
    // intervention). We can get rid of the browser_default eventually, but
    // need to take care that SyncServiceImpl doesn't get tripped up between
    // those two cases. Bug 88109.
    bool is_auto_start = browser_defaults::kSyncAutoStarts;
    init_params.start_behavior = is_auto_start
                                     ? syncer::SyncServiceImpl::AUTO_START
                                     : syncer::SyncServiceImpl::MANUAL_START;
  }

  auto sync_service =
      std::make_unique<syncer::SyncServiceImpl>(std::move(init_params));
  sync_service->Initialize();

  // Hook |sync_service| into PersonalDataManager (a circular dependency).
  autofill::PersonalDataManager* pdm =
      autofill::PersonalDataManagerFactory::GetForProfile(profile);
  pdm->OnSyncServiceInitialized(sync_service.get());

  // Notify PasswordStore of complete initialisation to resolve a circular
  // dependency.
  auto password_store = PasswordStoreFactory::GetForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS);
  // PasswordStoreInterface may be null in tests.
  if (password_store) {
    password_store->OnSyncServiceInitialized(sync_service.get());
  }

  return sync_service;
}

}  // anonymous namespace

// static
SyncServiceFactory* SyncServiceFactory::GetInstance() {
  return base::Singleton<SyncServiceFactory>::get();
}

// static
syncer::SyncService* SyncServiceFactory::GetForProfile(Profile* profile) {
  if (!syncer::IsSyncAllowedByFlag()) {
    return nullptr;
  }

  return static_cast<syncer::SyncService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
syncer::SyncServiceImpl*
SyncServiceFactory::GetAsSyncServiceImplForProfileForTesting(Profile* profile) {
  return static_cast<syncer::SyncServiceImpl*>(GetForProfile(profile));
}

SyncServiceFactory::SyncServiceFactory()
    : ProfileKeyedServiceFactory("SyncService") {
  // The SyncServiceImpl depends on various SyncableServices being around
  // when it is shut down.  Specify those dependencies here to build the proper
  // destruction order. Note that some of the dependencies are listed here but
  // actually plumbed in ChromeSyncClient, which this factory constructs.
  DependsOn(AboutSigninInternalsFactory::GetInstance());
  DependsOn(AccountPasswordStoreFactory::GetInstance());
  DependsOn(autofill::PersonalDataManagerFactory::GetInstance());
  DependsOn(BookmarkModelFactory::GetInstance());
  DependsOn(BookmarkSyncServiceFactory::GetInstance());
  DependsOn(BookmarkUndoServiceFactory::GetInstance());
  DependsOn(browser_sync::UserEventServiceFactory::GetInstance());
  DependsOn(ConsentAuditorFactory::GetInstance());
  DependsOn(DeviceInfoSyncServiceFactory::GetInstance());
  DependsOn(FaviconServiceFactory::GetInstance());
  DependsOn(gcm::GCMProfileServiceFactory::GetInstance());
  DependsOn(HistoryServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(ModelTypeStoreServiceFactory::GetInstance());
  DependsOn(PasswordStoreFactory::GetInstance());
  DependsOn(PowerBookmarkServiceFactory::GetInstance());
  DependsOn(SecurityEventRecorderFactory::GetInstance());
  DependsOn(SendTabToSelfSyncServiceFactory::GetInstance());
  DependsOn(SharingMessageBridgeFactory::GetInstance());
  DependsOn(SpellcheckServiceFactory::GetInstance());
  DependsOn(SyncInvalidationsServiceFactory::GetInstance());
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  DependsOn(SupervisedUserServiceFactory::GetInstance());
  DependsOn(SupervisedUserSettingsServiceFactory::GetInstance());
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)
  DependsOn(SessionSyncServiceFactory::GetInstance());
  DependsOn(TemplateURLServiceFactory::GetInstance());
#if !BUILDFLAG(IS_ANDROID)
  DependsOn(ThemeServiceFactory::GetInstance());
#endif  // !BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_WIN)
  DependsOn(SavedTabGroupServiceFactory::GetInstance());
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) ||
        // BUILDFLAG(IS_WIN)
  DependsOn(WebDataServiceFactory::GetInstance());
#if BUILDFLAG(ENABLE_EXTENSIONS)
  DependsOn(
      extensions::ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
  DependsOn(extensions::StorageFrontend::GetFactoryInstance());
  DependsOn(web_app::WebAppProviderFactory::GetInstance());
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
#if BUILDFLAG(IS_CHROMEOS_ASH)
  DependsOn(ash::SyncedPrintersManagerFactory::GetInstance());
  DependsOn(
      ash::printing::oauth2::AuthorizationZonesManagerFactory::GetInstance());
  DependsOn(DeskSyncServiceFactory::GetInstance());
  DependsOn(WifiConfigurationSyncServiceFactory::GetInstance());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

SyncServiceFactory::~SyncServiceFactory() = default;

KeyedService* SyncServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return BuildSyncService(context).release();
}

bool SyncServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

// static
bool SyncServiceFactory::HasSyncService(Profile* profile) {
  return GetInstance()->GetServiceForBrowserContext(profile, false) != nullptr;
}

// static
bool SyncServiceFactory::IsSyncAllowed(Profile* profile) {
  DCHECK(profile);

  if (HasSyncService(profile)) {
    syncer::SyncService* sync_service = GetForProfile(profile);
    return !sync_service->HasDisableReason(
        syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY);
  }

  // No SyncServiceImpl created yet - we don't want to create one, so just
  // infer the accessible state by looking at prefs/command line flags.
  syncer::SyncPrefs prefs(profile->GetPrefs());
  return syncer::IsSyncAllowedByFlag() &&
         (!prefs.IsSyncClientDisabledByPolicy() || prefs.IsLocalSyncEnabled());
}

// static
std::vector<const syncer::SyncService*>
SyncServiceFactory::GetAllSyncServices() {
  std::vector<Profile*> profiles =
      g_browser_process->profile_manager()->GetLoadedProfiles();
  std::vector<const syncer::SyncService*> sync_services;
  for (Profile* profile : profiles) {
    if (HasSyncService(profile)) {
      sync_services.push_back(GetForProfile(profile));
    }
  }
  return sync_services;
}

// static
BrowserContextKeyedServiceFactory::TestingFactory
SyncServiceFactory::GetDefaultFactory() {
  return base::BindRepeating(&BuildSyncService);
}
