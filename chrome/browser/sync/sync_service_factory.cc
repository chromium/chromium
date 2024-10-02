// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_service_factory.h"

#include <string>
#include <utility>

#include "base/containers/extend.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/android/webapk/webapk_sync_service_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/commerce/product_specifications/product_specifications_service_factory.h"
#include "chrome/browser/consent_auditor/consent_auditor_factory.h"
#include "chrome/browser/data_sharing/data_sharing_service_factory.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/metrics/variations/google_groups_manager_factory.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/password_receiver_service_factory.h"
#include "chrome/browser/password_manager/password_sender_service_factory.h"
#include "chrome/browser/password_manager/profile_password_store_factory.h"
#include "chrome/browser/plus_addresses/plus_address_setting_service_factory.h"
#include "chrome/browser/power_bookmarks/power_bookmark_service_factory.h"
#include "chrome/browser/prefs/pref_service_syncable_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/reading_list/reading_list_model_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/security_events/security_event_recorder_factory.h"
#include "chrome/browser/sharing/sharing_message_bridge_factory.h"
#include "chrome/browser/signin/about_signin_internals_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#include "chrome/browser/sync/account_bookmark_sync_service_factory.h"
#include "chrome/browser/sync/chrome_sync_client.h"
#include "chrome/browser/sync/chrome_sync_controller_builder.h"
#include "chrome/browser/sync/data_type_store_service_factory.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/browser/sync/local_or_syncable_bookmark_sync_service_factory.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/browser/sync/sync_invalidations_service_factory.h"
#include "chrome/browser/sync/user_event_service_factory.h"
#include "chrome/browser/tab_group_sync/feature_utils.h"
#include "chrome/browser/tab_group_sync/tab_group_trial.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/trusted_vault/trusted_vault_service_factory.h"
#include "chrome/browser/undo/bookmark_undo_service_factory.h"
#include "chrome/browser/webdata_services/web_data_service_factory.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_paths.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/browser_sync/common_controller_builder.h"
#include "components/password_manager/core/browser/sharing/password_receiver_service.h"
#include "components/plus_addresses/webdata/plus_address_webdata_service.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/spellcheck/browser/pref_names.h"
#include "components/sync/base/command_line_switches.h"
#include "components/sync/base/features.h"
#include "components/sync/service/sync_service_impl.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/variations/service/google_groups_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/buildflags/buildflags.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_sync_service.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_provider_factory.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "extensions/browser/api/storage/storage_frontend.h"  // nogncheck
#include "extensions/browser/extension_system_provider.h"     // nogncheck
#include "extensions/browser/extensions_browser_client.h"     // nogncheck
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/components/arc/arc_util.h"
#include "ash/constants/ash_features.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ash/app_list/arc/arc_package_syncable_service.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/floating_sso/floating_sso_service_factory.h"
#include "chrome/browser/ash/printing/oauth2/authorization_zones_manager_factory.h"
#include "chrome/browser/ash/printing/synced_printers_manager_factory.h"
#include "chrome/browser/sync/desk_sync_service_factory.h"
#include "chrome/browser/sync/wifi_configuration_sync_service_factory.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_WIN)
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_keyed_service.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_service_factory.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#elif BUILDFLAG(IS_ANDROID)
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "components/saved_tab_groups/public/features.h"
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) ||
        // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/android/webapk/webapk_sync_service.h"
#include "chrome/browser/android/webapk/webapk_sync_service_factory.h"

// Must come after other includes, because FromJniType() uses Profile.
#include "chrome/browser/sync/android/jni_headers/SyncServiceFactory_jni.h"
#else  // BUILDFLAG(IS_ANDROID)
#include "chrome/browser/webauthn/passkey_model_factory.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace {

bool ShouldSyncBrowserTypes() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return crosapi::browser_util::IsAshBrowserSyncEnabled();
#else
  return true;
#endif
}

syncer::DataTypeSet GetDisabledCommonDataTypes() {
  if (!ShouldSyncBrowserTypes()) {
    // If browser-sync is disabled (on ChromeOS Ash), most "common" data types
    // are disabled. These types will be synced in Lacros instead.
    return base::Difference(syncer::UserTypes(),
                            {syncer::DEVICE_INFO, syncer::USER_CONSENTS});
  }

  // Common case: No disabled types.
  return {};
}

// Returns TabGroupSyncService or null if the feature is disabled.
// Tab group sync is enabled via separate feature flags on different platforms.
tab_groups::TabGroupSyncService* GetTabGroupSyncService(Profile* profile) {
  CHECK(profile);
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_WIN)
  tab_groups::TabGroupSyncService* service =
      tab_groups::SavedTabGroupUtils::GetServiceForProfile(profile);
  CHECK(service);
  return service;
#elif BUILDFLAG(IS_ANDROID)
  const bool enable_tab_group_sync =
      tab_groups::IsTabGroupSyncEnabled(profile->GetPrefs()) &&
      !base::FeatureList::IsEnabled(
          tab_groups::kTabGroupSyncDisableNetworkLayer);
  tab_groups::TabGroupTrial::OnTabgroupSyncEnabled(enable_tab_group_sync);
  if (!enable_tab_group_sync) {
    return nullptr;
  }
  tab_groups::TabGroupSyncService* service =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile);
  CHECK(service);
  return service;
#else
  return nullptr;
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) ||
        // BUILDFLAG(IS_WIN)
}

syncer::DataTypeController::TypeVector CreateCommonControllers(
    Profile* profile,
    syncer::SyncService* sync_service) {
  scoped_refptr<autofill::AutofillWebDataService> profile_web_data_service =
      WebDataServiceFactory::GetAutofillWebDataForProfile(
          profile, ServiceAccessType::IMPLICIT_ACCESS);
  scoped_refptr<autofill::AutofillWebDataService> account_web_data_service =
      WebDataServiceFactory::GetAutofillWebDataForAccount(
          profile, ServiceAccessType::IMPLICIT_ACCESS);

  // This class assumes that the tasks posted by the profile and account storage
  // services will execute, in order, on the same sequence.
  // This DCHECK makes that assumption explicit.
#if DCHECK_IS_ON()
  if (account_web_data_service && profile_web_data_service) {
    profile_web_data_service->GetDBTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(
                       [](scoped_refptr<base::SequencedTaskRunner> ac_tr) {
                         CHECK(ac_tr->RunsTasksInCurrentSequence());
                       },
                       account_web_data_service->GetDBTaskRunner()));
  }
#endif  // DCHECK_IS_ON()

  browser_sync::CommonControllerBuilder builder;
  builder.SetAutofillWebDataService(content::GetUIThreadTaskRunner({}),
                                    profile_web_data_service,
                                    account_web_data_service);
  builder.SetBookmarkModel(BookmarkModelFactory::GetForBrowserContext(profile));
  builder.SetBookmarkSyncService(
      LocalOrSyncableBookmarkSyncServiceFactory::GetForProfile(profile),
      AccountBookmarkSyncServiceFactory::GetForProfile(profile));
  builder.SetConsentAuditor(ConsentAuditorFactory::GetForProfile(profile));
  builder.SetDataSharingService(
      data_sharing::DataSharingServiceFactory::GetForProfile(profile));
  builder.SetDeviceInfoSyncService(
      DeviceInfoSyncServiceFactory::GetForProfile(profile));
  builder.SetDualReadingListModel(
      ReadingListModelFactory::GetAsDualReadingListForBrowserContext(profile));
  builder.SetFaviconService(FaviconServiceFactory::GetForProfile(
      profile, ServiceAccessType::IMPLICIT_ACCESS));
  builder.SetGoogleGroupsManager(
      GoogleGroupsManagerFactory::GetForBrowserContext(profile));
  builder.SetHistoryService(HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS));
  builder.SetIdentityManager(IdentityManagerFactory::GetForProfile(profile));
  builder.SetDataTypeStoreService(
      DataTypeStoreServiceFactory::GetForProfile(profile));
#if !BUILDFLAG(IS_ANDROID)
  builder.SetPasskeyModel(
      base::FeatureList::IsEnabled(syncer::kSyncWebauthnCredentials)
          ? PasskeyModelFactory::GetForProfile(profile)
          : nullptr);
#endif  // !BUILDFLAG(IS_ANDROID)
  builder.SetPasswordReceiverService(
      PasswordReceiverServiceFactory::GetForProfile(profile));
  builder.SetPasswordSenderService(
      PasswordSenderServiceFactory::GetForProfile(profile));
  builder.SetPasswordStore(ProfilePasswordStoreFactory::GetForProfile(
                               profile, ServiceAccessType::IMPLICIT_ACCESS),
                           AccountPasswordStoreFactory::GetForProfile(
                               profile, ServiceAccessType::IMPLICIT_ACCESS));
  builder.SetPlusAddressServices(
      PlusAddressSettingServiceFactory::GetForBrowserContext(profile),
      WebDataServiceFactory::GetPlusAddressWebDataForProfile(
          profile, ServiceAccessType::IMPLICIT_ACCESS));
  builder.SetPowerBookmarkService(
      PowerBookmarkServiceFactory::GetForBrowserContext(profile));
  builder.SetPrefService(profile->GetPrefs());
  builder.SetPrefServiceSyncable(PrefServiceSyncableFromProfile(profile));
  builder.SetProductSpecificationsService(
      commerce::ProductSpecificationsServiceFactory::GetForBrowserContext(
          profile));
  builder.SetTabGroupSyncService(GetTabGroupSyncService(profile));
  builder.SetTemplateURLService(
#if BUILDFLAG(IS_ANDROID)
      nullptr
#else   // BUILDFLAG(IS_ANDROID)
      ShouldSyncBrowserTypes()
          ? TemplateURLServiceFactory::GetForProfile(profile)
          : nullptr
#endif  // BUILDFLAG(IS_ANDROID)
  );
  builder.SetSendTabToSelfSyncService(
      SendTabToSelfSyncServiceFactory::GetForProfile(profile));
  builder.SetSessionSyncService(
      SessionSyncServiceFactory::GetForProfile(profile));
  builder.SetSharingMessageBridge(
      ShouldSyncBrowserTypes()
          ? SharingMessageBridgeFactory::GetForBrowserContext(profile)
          : nullptr);
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  builder.SetSupervisedUserSettingsService(
      SupervisedUserSettingsServiceFactory::GetForKey(
          profile->GetProfileKey()));
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)
  builder.SetUserEventService(
      browser_sync::UserEventServiceFactory::GetForProfile(profile));

  return builder.Build(GetDisabledCommonDataTypes(), sync_service,
                       chrome::GetChannel());
}

syncer::DataTypeController::TypeVector CreateChromeControllers(
    Profile* profile,
    syncer::SyncService* sync_service) {
  ChromeSyncControllerBuilder builder;

  builder.SetDataTypeStoreService(
      DataTypeStoreServiceFactory::GetForProfile(profile));
  builder.SetSecurityEventRecorder(
      SecurityEventRecorderFactory::GetForProfile(profile));

#if BUILDFLAG(ENABLE_EXTENSIONS)
  builder.SetExtensionSyncService(ExtensionSyncService::Get(profile));
  builder.SetExtensionSystemProfile(profile);
  builder.SetThemeService(ThemeServiceFactory::GetForProfile(profile));
  builder.SetWebAppProvider(
      web_app::AreWebAppsEnabled(profile)
          ? web_app::WebAppProvider::GetForWebApps(profile)
          : nullptr);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(ENABLE_SPELLCHECK)
  builder.SetSpellcheckService(
      profile->GetPrefs()->GetBoolean(spellcheck::prefs::kSpellCheckEnable)
          ? SpellcheckServiceFactory::GetForContext(profile)
          : nullptr);
#endif  // BUILDFLAG(ENABLE_SPELLCHECK)

#if BUILDFLAG(IS_ANDROID)
  builder.SetWebApkSyncService(
      base::FeatureList::IsEnabled(syncer::kWebApkBackupAndRestoreBackend)
          ? webapk::WebApkSyncServiceFactory::GetForProfile(profile)
          : nullptr);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  const bool arc_enabled =
      arc::IsArcAllowedForProfile(profile) && !arc::IsArcAppSyncFlowDisabled();

  builder.SetAppListSyncableService(
      app_list::AppListSyncableServiceFactory::GetForProfile(profile));
  builder.SetAuthorizationZonesManager(
      ash::features::IsOAuthIppEnabled()
          ? ash::printing::oauth2::AuthorizationZonesManagerFactory::
                GetForBrowserContext(profile)
          : nullptr);
  builder.SetArcPackageSyncableService(
      arc_enabled ? arc::ArcPackageSyncableService::Get(profile) : nullptr,
      arc_enabled ? profile : nullptr);
  builder.SetDeskSyncService(DeskSyncServiceFactory::GetForProfile(profile));
  builder.SetFloatingSsoService(
      ash::features::IsFloatingSsoAllowed()
          ? ash::floating_sso::FloatingSsoServiceFactory::GetForProfile(profile)
          : nullptr);
  builder.SetOsPrefServiceSyncable(PrefServiceSyncableFromProfile(profile));
  builder.SetSyncedPrintersManager(
      ash::SyncedPrintersManagerFactory::GetForBrowserContext(profile));
  builder.SetWifiConfigurationSyncService(
      WifiConfigurationSyncServiceFactory::ShouldRunInProfile(profile)
          ? WifiConfigurationSyncServiceFactory::GetForProfile(profile,
                                                               /*create=*/true)
          : nullptr);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  return builder.Build(sync_service);
}

syncer::DataTypeController::TypeVector CreateControllers(
    Profile* profile,
    syncer::SyncService* sync_service) {
  syncer::DataTypeController::TypeVector controllers =
      CreateCommonControllers(profile, sync_service);
  base::Extend(controllers, CreateChromeControllers(profile, sync_service));
  return controllers;
}

std::unique_ptr<KeyedService> BuildSyncService(
    content::BrowserContext* context) {
  syncer::SyncServiceImpl::InitParams init_params;

  Profile* profile = Profile::FromBrowserContext(context);

  // Incognito, guest, or system profiles aren't relevant for Sync, and
  // no SyncService should be created for those types of profiles.
  CHECK(profiles::IsRegularUserProfile(profile));

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
  // TODO(crbug.com/40118868): Reassess whether the following block needs to be
  // included in lacros-chrome once build flag switch of lacros-chrome is
  // complete.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS))
  syncer::SyncPrefs prefs(profile->GetPrefs());
  local_sync_backend_enabled = prefs.IsLocalSyncEnabled();
  base::UmaHistogramBoolean("Sync.Local.Enabled2", local_sync_backend_enabled);

  if (local_sync_backend_enabled) {
    base::FilePath local_sync_backend_folder =
        init_params.sync_client->GetLocalSyncBackendFolder();

    // If the user has not specified a folder and we can't get the default
    // roaming profile location the sync service will not be created.
    base::UmaHistogramBoolean("Sync.Local.RoamingProfileUnavailable2",
                              local_sync_backend_folder.empty());
    if (local_sync_backend_folder.empty()) {
      return nullptr;
    }
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
  }

  auto sync_service =
      std::make_unique<syncer::SyncServiceImpl>(std::move(init_params));
  sync_service->Initialize(CreateControllers(profile, sync_service.get()));

  // Notify the PasswordStore of complete initialisation to resolve a circular
  // dependency.
  auto profile_password_store = ProfilePasswordStoreFactory::GetForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS);
  // PasswordStoreInterface may be null in tests.
  if (profile_password_store) {
    profile_password_store->OnSyncServiceInitialized(sync_service.get());
  }

  auto account_password_store = AccountPasswordStoreFactory::GetForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS);

  if (account_password_store) {
    account_password_store->OnSyncServiceInitialized(sync_service.get());
  }

  // Notify PasswordReceiverService of complete initialization to resolve a
  // circular dependency.
  password_manager::PasswordReceiverService* password_receiver_service =
      PasswordReceiverServiceFactory::GetForProfile(profile);
  if (password_receiver_service) {
    password_receiver_service->OnSyncServiceInitialized(sync_service.get());
  }

  SendTabToSelfSyncServiceFactory::GetForProfile(profile)
      ->OnSyncServiceInitialized(sync_service.get());

  // Allow sync_preferences/ components to use SyncService.
  sync_preferences::PrefServiceSyncable* pref_service =
      PrefServiceSyncableFromProfile(profile);
  pref_service->OnSyncServiceInitialized(sync_service.get());

  if (GoogleGroupsManager* groups_updater_service =
          GoogleGroupsManagerFactory::GetForBrowserContext(profile)) {
    groups_updater_service->OnSyncServiceInitialized(sync_service.get());
  }

  return sync_service;
}

}  // namespace

// static
SyncServiceFactory* SyncServiceFactory::GetInstance() {
  static base::NoDestructor<SyncServiceFactory> instance;
  return instance.get();
}

// static
syncer::SyncService* SyncServiceFactory::GetForProfile(Profile* profile) {
  if (!syncer::IsSyncAllowedByFlag()) {
    return nullptr;
  }

  return static_cast<syncer::SyncService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
syncer::SyncServiceImpl*
SyncServiceFactory::GetAsSyncServiceImplForProfileForTesting(Profile* profile) {
  return static_cast<syncer::SyncServiceImpl*>(GetForProfile(profile));
}

SyncServiceFactory::SyncServiceFactory()
    : ProfileKeyedServiceFactory(
          "SyncService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {
  // The SyncServiceImpl depends on various KeyedServices being around
  // when it is shut down.  Specify those dependencies here to build the proper
  // destruction order. Note that some of the dependencies are listed here but
  // actually plumbed in ChromeSyncClient, which this factory constructs.
  DependsOn(AboutSigninInternalsFactory::GetInstance());
  DependsOn(AccountBookmarkSyncServiceFactory::GetInstance());
  DependsOn(AccountPasswordStoreFactory::GetInstance());
  DependsOn(BookmarkModelFactory::GetInstance());
  DependsOn(BookmarkUndoServiceFactory::GetInstance());
  DependsOn(browser_sync::UserEventServiceFactory::GetInstance());
  DependsOn(ConsentAuditorFactory::GetInstance());
  DependsOn(DataTypeStoreServiceFactory::GetInstance());
  DependsOn(DeviceInfoSyncServiceFactory::GetInstance());
  DependsOn(data_sharing::DataSharingServiceFactory::GetInstance());
  DependsOn(FaviconServiceFactory::GetInstance());
  DependsOn(gcm::GCMProfileServiceFactory::GetInstance());
  DependsOn(GoogleGroupsManagerFactory::GetInstance());
  DependsOn(HistoryServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(LocalOrSyncableBookmarkSyncServiceFactory::GetInstance());
#if !BUILDFLAG(IS_ANDROID)
  DependsOn(PasskeyModelFactory::GetInstance());
#endif  // !BUILDFLAG(IS_ANDROID)
  DependsOn(PasswordReceiverServiceFactory::GetInstance());
  DependsOn(PasswordSenderServiceFactory::GetInstance());
  DependsOn(PlusAddressSettingServiceFactory::GetInstance());
  DependsOn(commerce::ProductSpecificationsServiceFactory::GetInstance());
  DependsOn(ProfilePasswordStoreFactory::GetInstance());
  DependsOn(PowerBookmarkServiceFactory::GetInstance());
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_WIN)
  DependsOn(tab_groups::SavedTabGroupServiceFactory::GetInstance());
#elif BUILDFLAG(IS_ANDROID)
  DependsOn(tab_groups::TabGroupSyncServiceFactory::GetInstance());
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) ||
        // BUILDFLAG(IS_WIN)
  DependsOn(SecurityEventRecorderFactory::GetInstance());
  DependsOn(SendTabToSelfSyncServiceFactory::GetInstance());
  DependsOn(SharingMessageBridgeFactory::GetInstance());
  DependsOn(SpellcheckServiceFactory::GetInstance());
  DependsOn(SyncInvalidationsServiceFactory::GetInstance());
  DependsOn(SupervisedUserSettingsServiceFactory::GetInstance());
  DependsOn(SessionSyncServiceFactory::GetInstance());
  DependsOn(TemplateURLServiceFactory::GetInstance());
#if !BUILDFLAG(IS_ANDROID)
  DependsOn(ThemeServiceFactory::GetInstance());
#endif  // !BUILDFLAG(IS_ANDROID)
  DependsOn(TrustedVaultServiceFactory::GetInstance());
#if BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(syncer::kWebApkBackupAndRestoreBackend)) {
    DependsOn(webapk::WebApkSyncServiceFactory::GetInstance());
  }
#endif  // BUILDFLAG(IS_ANDROID)
  DependsOn(WebDataServiceFactory::GetInstance());

#if BUILDFLAG(ENABLE_EXTENSIONS)
  DependsOn(
      extensions::ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
  DependsOn(extensions::StorageFrontend::GetFactoryInstance());
  DependsOn(web_app::WebAppProviderFactory::GetInstance());
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
#if BUILDFLAG(IS_CHROMEOS_ASH)
  DependsOn(app_list::AppListSyncableServiceFactory::GetInstance());
  DependsOn(
      ash::printing::oauth2::AuthorizationZonesManagerFactory::GetInstance());
  DependsOn(DeskSyncServiceFactory::GetInstance());
  if (ash::features::IsFloatingSsoAllowed()) {
    DependsOn(ash::floating_sso::FloatingSsoServiceFactory::GetInstance());
  }
  DependsOn(ash::SyncedPrintersManagerFactory::GetInstance());
  DependsOn(WifiConfigurationSyncServiceFactory::GetInstance());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

SyncServiceFactory::~SyncServiceFactory() = default;

std::unique_ptr<KeyedService>
SyncServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return BuildSyncService(context);
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

#if BUILDFLAG(IS_ANDROID)
static base::android::ScopedJavaLocalRef<jobject>
JNI_SyncServiceFactory_GetForProfile(JNIEnv* env, Profile* profile) {
  DCHECK(profile);

  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile);
  if (!sync_service) {
    return base::android::ScopedJavaLocalRef<jobject>();
  }
  return sync_service->GetJavaObject();
}
#endif  // BUILDFLAG(IS_ANDROID)
