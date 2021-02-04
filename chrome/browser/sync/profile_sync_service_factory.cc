// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/profile_sync_service_factory.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
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
#include "chrome/browser/policy/profile_policy_connector.h"
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
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/network_time/network_time_tracker.h"
#include "components/sync/driver/profile_sync_service.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/buildflags/buildflags.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

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
#include "ash/constants/ash_features.h"
#include "chrome/browser/chromeos/printing/synced_printers_manager_factory.h"
#include "chrome/browser/sync/wifi_configuration_sync_service_factory.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace {

void UpdateNetworkTimeOnUIThread(base::Time network_time,
                                 base::TimeDelta resolution,
                                 base::TimeDelta latency,
                                 base::TimeTicks post_time) {
  g_browser_process->network_time_tracker()->UpdateNetworkTime(
      network_time, resolution, latency, post_time);
}

void UpdateNetworkTime(const base::Time& network_time,
                       const base::TimeDelta& resolution,
                       const base::TimeDelta& latency) {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&UpdateNetworkTimeOnUIThread, network_time,
                                resolution, latency, base::TimeTicks::Now()));
}

}  // anonymous namespace

// static
ProfileSyncServiceFactory* ProfileSyncServiceFactory::GetInstance() {
  return base::Singleton<ProfileSyncServiceFactory>::get();
}

// static
syncer::SyncService* ProfileSyncServiceFactory::GetForProfile(
    Profile* profile) {
  if (!switches::IsSyncAllowedByFlag()) {
    return nullptr;
  }

  return static_cast<syncer::SyncService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
syncer::ProfileSyncService*
ProfileSyncServiceFactory::GetAsProfileSyncServiceForProfile(Profile* profile) {
  return static_cast<syncer::ProfileSyncService*>(GetForProfile(profile));
}

content::BrowserContext* ProfileSyncServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  if (context->IsOffTheRecord())
    return nullptr;
  if (Profile::FromBrowserContext(context)->IsEphemeralGuestProfile())
    return nullptr;
  return context;
}

ProfileSyncServiceFactory::ProfileSyncServiceFactory()
    : BrowserContextKeyedServiceFactory(
        "ProfileSyncService",
        BrowserContextDependencyManager::GetInstance()) {
  // The ProfileSyncService depends on various SyncableServices being around
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
  DependsOn(SyncInvalidationsServiceFactory::GetInstance());
  DependsOn(ModelTypeStoreServiceFactory::GetInstance());
  DependsOn(PasswordStoreFactory::GetInstance());
  DependsOn(SecurityEventRecorderFactory::GetInstance());
  DependsOn(SendTabToSelfSyncServiceFactory::GetInstance());
  DependsOn(SharingMessageBridgeFactory::GetInstance());
  DependsOn(SpellcheckServiceFactory::GetInstance());
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  DependsOn(SupervisedUserServiceFactory::GetInstance());
  DependsOn(SupervisedUserSettingsServiceFactory::GetInstance());
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)
  DependsOn(SessionSyncServiceFactory::GetInstance());
  DependsOn(TemplateURLServiceFactory::GetInstance());
#if !defined(OS_ANDROID)
  DependsOn(ThemeServiceFactory::GetInstance());
#endif  // !defined(OS_ANDROID)
  DependsOn(WebDataServiceFactory::GetInstance());
#if BUILDFLAG(ENABLE_EXTENSIONS)
  DependsOn(
      extensions::ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
  DependsOn(extensions::StorageFrontend::GetFactoryInstance());
  DependsOn(web_app::WebAppProviderFactory::GetInstance());
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
#if BUILDFLAG(IS_CHROMEOS_ASH)
  DependsOn(chromeos::SyncedPrintersManagerFactory::GetInstance());
  DependsOn(WifiConfigurationSyncServiceFactory::GetInstance());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

ProfileSyncServiceFactory::~ProfileSyncServiceFactory() = default;

KeyedService* ProfileSyncServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  syncer::ProfileSyncService::InitParams init_params;

  Profile* profile = Profile::FromBrowserContext(context);

  std::unique_ptr<browser_sync::ChromeSyncClient> sync_client =
      client_factory_
          ? client_factory_->Run(profile)
          : std::make_unique<browser_sync::ChromeSyncClient>(profile);

  init_params.sync_client = std::move(sync_client);
  init_params.network_time_update_callback =
      base::BindRepeating(&UpdateNetworkTime);
  init_params.url_loader_factory =
      content::BrowserContext::GetDefaultStoragePartition(profile)
          ->GetURLLoaderFactoryForBrowserProcess();
  init_params.network_connection_tracker =
      content::GetNetworkConnectionTracker();
  init_params.channel = chrome::GetChannel();
  init_params.debug_identifier = profile->GetDebugName();

  init_params.policy_service =
      profile->GetProfilePolicyConnector()->policy_service();
  bool local_sync_backend_enabled = false;

// Only check the local sync backend pref on the supported platforms of
// Windows, Mac and Linux.
// TODO(crbug.com/1052397): Reassess whether the following block needs to be
// included
// in lacros-chrome once build flag switch of lacros-chrome is
// complete.
#if defined(OS_WIN) || defined(OS_MAC) || \
    (defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
  syncer::SyncPrefs prefs(profile->GetPrefs());
  local_sync_backend_enabled = prefs.IsLocalSyncEnabled();
  UMA_HISTOGRAM_BOOLEAN("Sync.Local.Enabled", local_sync_backend_enabled);

  if (local_sync_backend_enabled) {
    base::FilePath local_sync_backend_folder =
        init_params.sync_client->GetLocalSyncBackendFolder();

    // If the user has not specified a folder and we can't get the default
    // roaming profile location the sync service will not be created.
    UMA_HISTOGRAM_BOOLEAN("Sync.Local.RoamingProfileUnavailable",
                          local_sync_backend_folder.empty());
    if (local_sync_backend_folder.empty())
      return nullptr;

    init_params.start_behavior = syncer::ProfileSyncService::AUTO_START;
  }
#endif  // defined(OS_WIN) || defined(OS_MAC) || (defined(OS_LINUX) ||
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
    // need to take care that ProfileSyncService doesn't get tripped up between
    // those two cases. Bug 88109.
    bool is_auto_start = browser_defaults::kSyncAutoStarts;
#if BUILDFLAG(IS_CHROMEOS_ASH)
    if (chromeos::features::IsSplitSettingsSyncEnabled())
      is_auto_start = false;
#endif
    init_params.start_behavior = is_auto_start
                                     ? syncer::ProfileSyncService::AUTO_START
                                     : syncer::ProfileSyncService::MANUAL_START;
  }

  auto pss =
      std::make_unique<syncer::ProfileSyncService>(std::move(init_params));
  pss->Initialize();

  // Hook PSS into PersonalDataManager (a circular dependency).
  autofill::PersonalDataManager* pdm =
      autofill::PersonalDataManagerFactory::GetForProfile(profile);
  pdm->OnSyncServiceInitialized(pss.get());

  return pss.release();
}

// static
bool ProfileSyncServiceFactory::HasSyncService(Profile* profile) {
  return GetInstance()->GetServiceForBrowserContext(profile, false) != nullptr;
}

// static
bool ProfileSyncServiceFactory::IsSyncAllowed(Profile* profile) {
  DCHECK(profile);

  if (HasSyncService(profile)) {
    syncer::SyncService* sync_service = GetForProfile(profile);
    return !sync_service->HasDisableReason(
               syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY);
  }

  // No ProfileSyncService created yet - we don't want to create one, so just
  // infer the accessible state by looking at prefs/command line flags.
  syncer::SyncPrefs prefs(profile->GetPrefs());
  return switches::IsSyncAllowedByFlag() &&
         (!prefs.IsManaged() || prefs.IsLocalSyncEnabled());
}

// static
std::vector<const syncer::SyncService*>
ProfileSyncServiceFactory::GetAllSyncServices() {
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
void ProfileSyncServiceFactory::SetSyncClientFactoryForTest(
    SyncClientFactory* client_factory) {
  client_factory_ = client_factory;
}

// static
ProfileSyncServiceFactory::SyncClientFactory*
    ProfileSyncServiceFactory::client_factory_ = nullptr;
