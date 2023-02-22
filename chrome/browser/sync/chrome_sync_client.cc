// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/chrome_sync_client.h"

#include <memory>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/syslog_logging.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/consent_auditor/consent_auditor_factory.h"
#include "chrome/browser/dom_distiller/dom_distiller_service_factory.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/invalidation/profile_invalidation_provider_factory.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/power_bookmarks/power_bookmark_service_factory.h"
#include "chrome/browser/prefs/pref_service_syncable_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/reading_list/reading_list_model_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/security_events/security_event_recorder.h"
#include "chrome/browser/security_events/security_event_recorder_factory.h"
#include "chrome/browser/sharing/sharing_message_bridge.h"
#include "chrome/browser/sharing/sharing_message_bridge_factory.h"
#include "chrome/browser/sharing/sharing_message_model_type_controller.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/bookmark_sync_service_factory.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/browser/sync/model_type_store_service_factory.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/browser/sync/sync_invalidations_service_factory.h"
#include "chrome/browser/sync/user_event_service_factory.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/themes/theme_syncable_service.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/browser/web_data_service_factory.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "components/autofill/core/browser/webdata/autocomplete_sync_bridge.h"
#include "components/autofill/core/browser/webdata/autofill_profile_sync_bridge.h"
#include "components/autofill/core/browser/webdata/autofill_wallet_metadata_sync_bridge.h"
#include "components/autofill/core/browser/webdata/autofill_wallet_sync_bridge.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/browser_sync/browser_sync_switches.h"
#include "components/browser_sync/sync_api_component_factory_impl.h"
#include "components/consent_auditor/consent_auditor.h"
#include "components/desks_storage/core/desk_sync_service.h"
#include "components/dom_distiller/core/dom_distiller_service.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/common/pref_names.h"
#include "components/invalidation/impl/invalidation_switches.h"
#include "components/invalidation/impl/profile_invalidation_provider.h"
#include "components/metrics/demographics/user_demographics.h"
#include "components/password_manager/core/browser/password_store_interface.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url_service.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "components/supervised_user/core/common/buildflags.h"
#include "components/sync/base/features.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/base/sync_util.h"
#include "components/sync/driver/model_type_controller.h"
#include "components/sync/driver/sync_api_component_factory.h"
#include "components/sync/driver/syncable_service_based_model_type_controller.h"
#include "components/sync/model/forwarding_model_type_controller_delegate.h"
#include "components/sync/model/model_type_controller_delegate.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/model/model_type_store_service.h"
#include "components/sync_bookmarks/bookmark_sync_service.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/sync_sessions/session_sync_service.h"
#include "components/sync_user_events/user_event_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/api/storage/backend_task_runner.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/api/storage/settings_sync_util.h"
#include "chrome/browser/extensions/extension_sync_service.h"
#include "chrome/browser/sync/glue/extension_model_type_controller.h"
#include "chrome/browser/sync/glue/extension_setting_model_type_controller.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#include "components/supervised_user/core/browser/supervised_user_settings_service.h"
#include "components/supervised_user/core/browser/supervised_user_sync_model_type_controller.h"
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)

#if BUILDFLAG(ENABLE_SPELLCHECK)
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "components/spellcheck/browser/pref_names.h"
#endif  // BUILDFLAG(ENABLE_SPELLCHECK)

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/sync/trusted_vault_client_android.h"
#else
#include "components/sync/trusted_vault/standalone_trusted_vault_client.h"  // nogncheck
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_WIN)
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_keyed_service.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_service_factory.h"
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) ||
        // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/components/arc/arc_util.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ash/app_list/arc/arc_package_sync_model_type_controller.h"
#include "chrome/browser/ash/app_list/arc/arc_package_syncable_service.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/printing/oauth2/authorization_zones_manager.h"
#include "chrome/browser/ash/printing/oauth2/authorization_zones_manager_factory.h"
#include "chrome/browser/ash/printing/printers_sync_bridge.h"
#include "chrome/browser/ash/printing/synced_printers_manager.h"
#include "chrome/browser/ash/printing/synced_printers_manager_factory.h"
#include "chrome/browser/ash/sync/os_syncable_service_model_type_controller.h"
#include "chrome/browser/sync/desk_sync_service_factory.h"
#include "chrome/browser/sync/wifi_configuration_sync_service_factory.h"
#include "chromeos/ash/components/sync_wifi/wifi_configuration_sync_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

using content::BrowserThread;

#if BUILDFLAG(ENABLE_EXTENSIONS)
using browser_sync::ExtensionModelTypeController;
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

namespace browser_sync {

namespace {

#if !BUILDFLAG(IS_ANDROID)
constexpr base::FilePath::CharType kTrustedVaultFilename[] =
    FILE_PATH_LITERAL("trusted_vault.pb");
constexpr base::FilePath::CharType kDeprecatedTrustedVaultFilename[] =
    FILE_PATH_LITERAL("Trusted Vault");
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN)
constexpr base::FilePath::CharType kLoopbackServerBackendFilename[] =
    FILE_PATH_LITERAL("profile.pb");
#endif  // BUILDFLAG(IS_WIN)

base::WeakPtr<syncer::SyncableService> GetWeakPtrOrNull(
    syncer::SyncableService* service) {
  return service ? service->AsWeakPtr() : nullptr;
}

base::RepeatingClosure GetDumpStackClosure() {
  return base::BindRepeating(&syncer::ReportUnrecoverableError,
                             chrome::GetChannel());
}

bool ShouldSyncBrowserTypes() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return crosapi::browser_util::IsAshBrowserSyncEnabled();
#else
  return true;
#endif
}

syncer::ModelTypeSet GetDisabledCommonDataTypes() {
  if (!ShouldSyncBrowserTypes()) {
    // If browser-sync is disabled (on ChromeOS Ash), most "common" data types
    // are disabled. These types will be synced in Lacros instead.
    return base::Difference(syncer::UserTypes(),
                            {syncer::DEVICE_INFO, syncer::USER_CONSENTS});
  }

  // Common case: No disabled types.
  return {};
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
// App sync is enabled by default, with the exception of Lacros secondary
// profiles.
bool IsAppSyncEnabled(Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!profile->IsMainProfile() &&
      !web_app::IsMainProfileCheckSkippedForTesting()) {
    return false;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  return true;
}

bool ShouldSyncAppsTypesInTransportMode() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // When apps sync controlled by Ash Sync settings, allow running apps-related
  // types (WEB_APPS, APPS and APP_SETTINGS) in transport-only mode using the
  // same `delegate`.
  return base::FeatureList::IsEnabled(syncer::kSyncChromeOSAppsToggleSharing);
#else
  return false;
#endif
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

}  // namespace

ChromeSyncClient::ChromeSyncClient(Profile* profile)
    : profile_(profile), extensions_activity_monitor_(profile) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  profile_web_data_service_ =
      WebDataServiceFactory::GetAutofillWebDataForProfile(
          profile_, ServiceAccessType::IMPLICIT_ACCESS);
  account_web_data_service_ =
      base::FeatureList::IsEnabled(
          autofill::features::kAutofillEnableAccountWalletStorage)
          ? WebDataServiceFactory::GetAutofillWebDataForAccount(
                profile_, ServiceAccessType::IMPLICIT_ACCESS)
          : nullptr;
  web_data_service_thread_ = profile_web_data_service_
                                 ? profile_web_data_service_->GetDBTaskRunner()
                                 : nullptr;

  // This class assumes that the database thread is the same across the profile
  // and account storage. This DCHECK makes that assumption explicit.
  DCHECK(!account_web_data_service_ ||
         web_data_service_thread_ ==
             account_web_data_service_->GetDBTaskRunner());
  profile_password_store_ = PasswordStoreFactory::GetForProfile(
      profile_, ServiceAccessType::IMPLICIT_ACCESS);
  account_password_store_ = AccountPasswordStoreFactory::GetForProfile(
      profile_, ServiceAccessType::IMPLICIT_ACCESS);

  component_factory_ = std::make_unique<SyncApiComponentFactoryImpl>(
      this, chrome::GetChannel(), content::GetUIThreadTaskRunner({}),
      web_data_service_thread_, profile_web_data_service_,
      account_web_data_service_, profile_password_store_,
      account_password_store_,
      BookmarkSyncServiceFactory::GetForProfile(profile_),
      PowerBookmarkServiceFactory::GetForBrowserContext(profile_));

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);

#if BUILDFLAG(IS_ANDROID)
  trusted_vault_client_ = std::make_unique<TrustedVaultClientAndroid>(
      /*gaia_account_info_by_gaia_id_cb=*/base::BindRepeating(
          [](signin::IdentityManager* identity_manager,
             const std::string& gaia_id) -> CoreAccountInfo {
            return identity_manager->FindExtendedAccountInfoByGaiaId(gaia_id);
          },
          identity_manager));
#else
  trusted_vault_client_ =
      std::make_unique<syncer::StandaloneTrustedVaultClient>(
          profile_->GetPath().Append(kTrustedVaultFilename),
          profile_->GetPath().Append(kDeprecatedTrustedVaultFilename),
          identity_manager,
          profile_->GetDefaultStoragePartition()
              ->GetURLLoaderFactoryForBrowserProcess());
#endif  // BUILDFLAG(IS_ANDROID)
}

ChromeSyncClient::~ChromeSyncClient() = default;

PrefService* ChromeSyncClient::GetPrefService() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return profile_->GetPrefs();
}

signin::IdentityManager* ChromeSyncClient::GetIdentityManager() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return IdentityManagerFactory::GetForProfile(profile_);
}

base::FilePath ChromeSyncClient::GetLocalSyncBackendFolder() {
  base::FilePath local_sync_backend_folder =
      GetPrefService()->GetFilePath(syncer::prefs::kLocalSyncBackendDir);

#if BUILDFLAG(IS_WIN)
  if (local_sync_backend_folder.empty()) {
    if (!base::PathService::Get(chrome::DIR_ROAMING_USER_DATA,
                                &local_sync_backend_folder)) {
      SYSLOG(WARNING) << "Local sync can not get the roaming profile folder.";
      return base::FilePath();
    }
  }

  // This code as it is now will assume the same profile order is present on
  // all machines, which is not a given. It is to be defined if only the
  // Default profile should get this treatment or all profile as is the case
  // now.
  // TODO(pastarmovj): http://crbug.com/674928 Decide if only the Default one
  // should be considered roamed. For now the code assumes all profiles are
  // created in the same order on all machines.
  local_sync_backend_folder =
      local_sync_backend_folder.Append(profile_->GetBaseName());
  local_sync_backend_folder =
      local_sync_backend_folder.Append(kLoopbackServerBackendFilename);
#endif  // BUILDFLAG(IS_WIN)

  return local_sync_backend_folder;
}

syncer::ModelTypeStoreService* ChromeSyncClient::GetModelTypeStoreService() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return ModelTypeStoreServiceFactory::GetForProfile(profile_);
}

syncer::DeviceInfoSyncService* ChromeSyncClient::GetDeviceInfoSyncService() {
  return DeviceInfoSyncServiceFactory::GetForProfile(profile_);
}

favicon::FaviconService* ChromeSyncClient::GetFaviconService() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return FaviconServiceFactory::GetForProfile(
      profile_, ServiceAccessType::IMPLICIT_ACCESS);
}

history::HistoryService* ChromeSyncClient::GetHistoryService() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return HistoryServiceFactory::GetForProfile(
      profile_, ServiceAccessType::EXPLICIT_ACCESS);
}

ReadingListModel* ChromeSyncClient::GetReadingListModel() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return ReadingListModelFactory::GetForBrowserContext(profile_);
}

send_tab_to_self::SendTabToSelfSyncService*
ChromeSyncClient::GetSendTabToSelfSyncService() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return SendTabToSelfSyncServiceFactory::GetForProfile(profile_);
}

sync_preferences::PrefServiceSyncable*
ChromeSyncClient::GetPrefServiceSyncable() {
  return PrefServiceSyncableFromProfile(profile_);
}

sync_sessions::SessionSyncService* ChromeSyncClient::GetSessionSyncService() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return SessionSyncServiceFactory::GetForProfile(profile_);
}

syncer::DataTypeController::TypeVector
ChromeSyncClient::CreateDataTypeControllers(syncer::SyncService* sync_service) {
  syncer::DataTypeController::TypeVector controllers =
      component_factory_->CreateCommonDataTypeControllers(
          GetDisabledCommonDataTypes(), sync_service);

  const base::RepeatingClosure dump_stack = GetDumpStackClosure();

  syncer::RepeatingModelTypeStoreFactory model_type_store_factory =
      GetModelTypeStoreService()->GetStoreFactory();

  if (ShouldSyncBrowserTypes()) {
    syncer::ModelTypeControllerDelegate* security_events_delegate =
        SecurityEventRecorderFactory::GetForProfile(profile_)
            ->GetControllerDelegate()
            .get();
    // Forward both full-sync and transport-only modes to the same delegate,
    // since behavior for SECURITY_EVENTS does not differ.
    controllers.push_back(std::make_unique<syncer::ModelTypeController>(
        syncer::SECURITY_EVENTS,
        /*delegate_for_full_sync_mode=*/
        std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
            security_events_delegate),
        /*delegate_for_transport_mode=*/
        std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
            security_events_delegate)));

    // Forward both full-sync and transport-only modes to the same delegate,
    // since behavior for SHARING_MESSAGE does not differ. They both do not
    // store data on persistent storage.
    syncer::ModelTypeControllerDelegate* sharing_message_delegate =
        GetControllerDelegateForModelType(syncer::SHARING_MESSAGE).get();
    controllers.push_back(std::make_unique<SharingMessageModelTypeController>(
        /*delegate_for_full_sync_mode=*/
        std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
            sharing_message_delegate),
        /*delegate_for_transport_mode=*/
        std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
            sharing_message_delegate)));

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
    // |profile_| must not be null and must outlive this controller.
    controllers.push_back(
        std::make_unique<SupervisedUserSyncModelTypeController>(
            syncer::SUPERVISED_USER_SETTINGS,
            base::BindRepeating(&Profile::IsChild, base::Unretained(profile_)),
            dump_stack, model_type_store_factory,
            GetSyncableServiceForType(syncer::SUPERVISED_USER_SETTINGS)));
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)

#if BUILDFLAG(ENABLE_EXTENSIONS)
    // Extension sync is enabled by default.
    controllers.push_back(std::make_unique<ExtensionModelTypeController>(
        syncer::EXTENSIONS, model_type_store_factory,
        GetSyncableServiceForType(syncer::EXTENSIONS), dump_stack,
        ExtensionModelTypeController::DelegateMode::kFullSyncModeOnly,
        profile_));

    // Extension setting sync is enabled by default.
    controllers.push_back(std::make_unique<ExtensionSettingModelTypeController>(
        syncer::EXTENSION_SETTINGS, model_type_store_factory,
        extensions::settings_sync_util::GetSyncableServiceProvider(
            profile_, syncer::EXTENSION_SETTINGS),
        dump_stack,
        ExtensionSettingModelTypeController::DelegateMode::kFullSyncModeOnly,
        profile_));

    if (IsAppSyncEnabled(profile_)) {
      controllers.push_back(CreateAppsModelTypeController());

      controllers.push_back(CreateAppSettingsModelTypeController(sync_service));

      if (web_app::AreWebAppsEnabled(profile_) &&
          web_app::WebAppProvider::GetForWebApps(profile_)) {
        controllers.push_back(CreateWebAppsModelTypeController());
      }
    }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if !BUILDFLAG(IS_ANDROID)
    // Theme sync is enabled by default.
    controllers.push_back(std::make_unique<ExtensionModelTypeController>(
        syncer::THEMES, model_type_store_factory,
        GetSyncableServiceForType(syncer::THEMES), dump_stack,
        ExtensionModelTypeController::DelegateMode::kFullSyncModeOnly,
        profile_));

    // Search Engine sync is enabled by default.
    controllers.push_back(
        std::make_unique<syncer::SyncableServiceBasedModelTypeController>(
            syncer::SEARCH_ENGINES, model_type_store_factory,
            GetSyncableServiceForType(syncer::SEARCH_ENGINES), dump_stack));
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_WIN)
    if (features::kTabGroupsSaveSyncIntegration.Get()) {
      controllers.push_back(std::make_unique<syncer::ModelTypeController>(
          syncer::SAVED_TAB_GROUP,
          std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
              GetControllerDelegateForModelType(syncer::SAVED_TAB_GROUP)
                  .get())));
    }
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) ||
        // BUILDFLAG(IS_WIN)

// Chrome prefers OS provided spell checkers where they exist. So only sync the
// custom dictionary on platforms that typically don't provide one.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
    // Dictionary sync is enabled by default.
    if (GetPrefService()->GetBoolean(spellcheck::prefs::kSpellCheckEnable)) {
      controllers.push_back(
          std::make_unique<syncer::SyncableServiceBasedModelTypeController>(
              syncer::DICTIONARY, model_type_store_factory,
              GetSyncableServiceForType(syncer::DICTIONARY), dump_stack));
    }
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Some profile types (e.g. sign-in screen) don't support app list.
  // Temporarily Disable AppListSyncableService for tablet form factor devices.
  // See crbug/1013732 for details.
  if (app_list::AppListSyncableServiceFactory::GetForProfile(profile_) &&
      !ash::switches::IsTabletFormFactor()) {
    // Runs in sync transport-mode and full-sync mode.
    controllers.push_back(
        std::make_unique<OsSyncableServiceModelTypeController>(
            syncer::APP_LIST, model_type_store_factory,
            GetSyncableServiceForType(syncer::APP_LIST), dump_stack,
            profile_->GetPrefs(), sync_service));
  }

  if (arc::IsArcAllowedForProfile(profile_) &&
      !arc::IsArcAppSyncFlowDisabled()) {
    controllers.push_back(std::make_unique<ArcPackageSyncModelTypeController>(
        model_type_store_factory,
        GetSyncableServiceForType(syncer::ARC_PACKAGE), dump_stack,
        sync_service, profile_));
  }
  controllers.push_back(std::make_unique<OsSyncableServiceModelTypeController>(
      syncer::OS_PREFERENCES, model_type_store_factory,
      GetSyncableServiceForType(syncer::OS_PREFERENCES), dump_stack,
      profile_->GetPrefs(), sync_service));
  controllers.push_back(std::make_unique<OsSyncableServiceModelTypeController>(
      syncer::OS_PRIORITY_PREFERENCES, model_type_store_factory,
      GetSyncableServiceForType(syncer::OS_PRIORITY_PREFERENCES), dump_stack,
      profile_->GetPrefs(), sync_service));

  syncer::ModelTypeControllerDelegate* printers_delegate =
      GetControllerDelegateForModelType(syncer::PRINTERS).get();
  controllers.push_back(std::make_unique<syncer::ModelTypeController>(
      syncer::PRINTERS,
      std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
          printers_delegate)));

  if (WifiConfigurationSyncServiceFactory::ShouldRunInProfile(profile_)) {
    syncer::ModelTypeControllerDelegate* wifi_configurations_delegate =
        GetControllerDelegateForModelType(syncer::WIFI_CONFIGURATIONS).get();
    controllers.push_back(std::make_unique<syncer::ModelTypeController>(
        syncer::WIFI_CONFIGURATIONS,
        std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
            wifi_configurations_delegate)));
  }
  syncer::ModelTypeControllerDelegate* workspace_desk_delegate =
      GetControllerDelegateForModelType(syncer::WORKSPACE_DESK).get();
  controllers.push_back(std::make_unique<syncer::ModelTypeController>(
      syncer::WORKSPACE_DESK,
      std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
          workspace_desk_delegate)));

  if (ash::features::IsOAuthIppEnabled()) {
    syncer::ModelTypeControllerDelegate*
        printers_authorization_servers_delegate =
            GetControllerDelegateForModelType(
                syncer::PRINTERS_AUTHORIZATION_SERVERS)
                .get();
    controllers.push_back(std::make_unique<syncer::ModelTypeController>(
        syncer::PRINTERS_AUTHORIZATION_SERVERS,
        std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
            printers_authorization_servers_delegate)));
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  return controllers;
}

syncer::TrustedVaultClient* ChromeSyncClient::GetTrustedVaultClient() {
  return trusted_vault_client_.get();
}

invalidation::InvalidationService* ChromeSyncClient::GetInvalidationService() {
  invalidation::ProfileInvalidationProvider* provider =
      invalidation::ProfileInvalidationProviderFactory::GetForProfile(profile_);

  if (provider) {
    return provider->GetInvalidationService();
  }
  return nullptr;
}

syncer::SyncInvalidationsService*
ChromeSyncClient::GetSyncInvalidationsService() {
  return SyncInvalidationsServiceFactory::GetForProfile(profile_);
}

scoped_refptr<syncer::ExtensionsActivity>
ChromeSyncClient::GetExtensionsActivity() {
  return extensions_activity_monitor_.GetExtensionsActivity();
}

base::WeakPtr<syncer::SyncableService>
ChromeSyncClient::GetSyncableServiceForType(syncer::ModelType type) {
  switch (type) {
    case syncer::SEARCH_ENGINES:
      return GetWeakPtrOrNull(
          TemplateURLServiceFactory::GetForProfile(profile_));
#if BUILDFLAG(ENABLE_EXTENSIONS)
    case syncer::APPS:
    case syncer::EXTENSIONS:
      return GetWeakPtrOrNull(ExtensionSyncService::Get(profile_));
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
#if BUILDFLAG(IS_CHROMEOS_ASH)
    case syncer::APP_LIST:
      return GetWeakPtrOrNull(
          app_list::AppListSyncableServiceFactory::GetForProfile(profile_));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if !BUILDFLAG(IS_ANDROID)
    case syncer::THEMES:
      return ThemeServiceFactory::GetForProfile(profile_)
          ->GetThemeSyncableService()
          ->AsWeakPtr();
#endif  // !BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(ENABLE_SPELLCHECK)
    case syncer::DICTIONARY: {
      SpellcheckService* spellcheck_service =
          SpellcheckServiceFactory::GetForContext(profile_);
      return spellcheck_service
                 ? spellcheck_service->GetCustomDictionary()->AsWeakPtr()
                 : nullptr;
    }
#endif  // BUILDFLAG(ENABLE_SPELLCHECK)
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
    case syncer::SUPERVISED_USER_SETTINGS:
      return SupervisedUserSettingsServiceFactory::GetForKey(
                 profile_->GetProfileKey())
          ->AsWeakPtr();
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)
#if BUILDFLAG(IS_CHROMEOS_ASH)
    case syncer::ARC_PACKAGE:
      return arc::ArcPackageSyncableService::Get(profile_)->AsWeakPtr();
    case syncer::OS_PREFERENCES:
    case syncer::OS_PRIORITY_PREFERENCES:
      return PrefServiceSyncableFromProfile(profile_)
          ->GetSyncableService(type)
          ->AsWeakPtr();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    default:
      NOTREACHED();
      return nullptr;
  }
}

base::WeakPtr<syncer::ModelTypeControllerDelegate>
ChromeSyncClient::GetControllerDelegateForModelType(syncer::ModelType type) {
  switch (type) {
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_WIN)
    case syncer::SAVED_TAB_GROUP: {
      DCHECK(features::kTabGroupsSaveSyncIntegration.Get());
      return SavedTabGroupServiceFactory::GetForProfile(profile_)
          ->bridge()
          ->change_processor()
          ->GetControllerDelegate();
    }
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) ||
        // BUILDFLAG(IS_WIN)
#if BUILDFLAG(IS_CHROMEOS_ASH)
    case syncer::PRINTERS:
      return ash::SyncedPrintersManagerFactory::GetForBrowserContext(profile_)
          ->GetSyncBridge()
          ->change_processor()
          ->GetControllerDelegate();
    case syncer::PRINTERS_AUTHORIZATION_SERVERS:
      return ash::printing::oauth2::AuthorizationZonesManagerFactory::
          GetForBrowserContext(profile_)
              ->GetModelTypeSyncBridge()
              ->change_processor()
              ->GetControllerDelegate();
    case syncer::WIFI_CONFIGURATIONS:
      return WifiConfigurationSyncServiceFactory::GetForProfile(profile_,
                                                                /*create=*/true)
          ->GetControllerDelegate();
    case syncer::WORKSPACE_DESK:
      return DeskSyncServiceFactory::GetForProfile(profile_)
          ->GetControllerDelegate();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    case syncer::SHARING_MESSAGE:
      return SharingMessageBridgeFactory::GetForBrowserContext(profile_)
          ->GetControllerDelegate();
    case syncer::USER_CONSENTS:
      return ConsentAuditorFactory::GetForProfile(profile_)
          ->GetControllerDelegate();
    case syncer::USER_EVENTS:
      return browser_sync::UserEventServiceFactory::GetForProfile(profile_)
          ->GetControllerDelegate();
#if BUILDFLAG(ENABLE_EXTENSIONS)
    case syncer::WEB_APPS: {
      auto* provider = web_app::WebAppProvider::GetForWebApps(profile_);

      // CreateWebAppsModelTypeController(), and therefore this code, should
      // never be called when GetForWebApps() returns nullptr.
      DCHECK(provider);
      DCHECK(web_app::AreWebAppsEnabled(profile_));

      return provider->sync_bridge_unsafe()
          .change_processor()
          ->GetControllerDelegate();
    }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
    // We don't exercise this function for certain datatypes, because their
    // controllers get the delegate elsewhere.
    case syncer::AUTOFILL:
    case syncer::AUTOFILL_PROFILE:
    case syncer::AUTOFILL_WALLET_DATA:
    case syncer::AUTOFILL_WALLET_METADATA:
    case syncer::BOOKMARKS:
    case syncer::DEVICE_INFO:
    case syncer::READING_LIST:
    case syncer::SECURITY_EVENTS:
    case syncer::SEND_TAB_TO_SELF:
    case syncer::SESSIONS:
    case syncer::TYPED_URLS:
      NOTREACHED();
      return base::WeakPtr<syncer::ModelTypeControllerDelegate>();

    default:
      NOTREACHED();
      return base::WeakPtr<syncer::ModelTypeControllerDelegate>();
  }
}

syncer::SyncApiComponentFactory*
ChromeSyncClient::GetSyncApiComponentFactory() {
  return component_factory_.get();
}

syncer::SyncTypePreferenceProvider* ChromeSyncClient::GetPreferenceProvider() {
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  return SupervisedUserServiceFactory::GetForProfile(profile_);
#else
  return nullptr;
#endif
}

void ChromeSyncClient::OnLocalSyncTransportDataCleared() {
  metrics::ClearDemographicsPrefs(profile_->GetPrefs());
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
std::unique_ptr<syncer::ModelTypeController>
ChromeSyncClient::CreateAppsModelTypeController() {
  auto delegate_mode =
      ExtensionModelTypeController::DelegateMode::kFullSyncModeOnly;
  if (ShouldSyncAppsTypesInTransportMode()) {
    delegate_mode = ExtensionModelTypeController::DelegateMode::
        kTransportModeWithSingleModel;
  }
  return std::make_unique<ExtensionModelTypeController>(
      syncer::APPS, GetModelTypeStoreService()->GetStoreFactory(),
      GetSyncableServiceForType(syncer::APPS), GetDumpStackClosure(),
      delegate_mode, profile_);
}

std::unique_ptr<syncer::ModelTypeController>
ChromeSyncClient::CreateAppSettingsModelTypeController(
    syncer::SyncService* sync_service) {
  auto delegate_mode =
      ExtensionSettingModelTypeController::DelegateMode::kFullSyncModeOnly;
  if (ShouldSyncAppsTypesInTransportMode()) {
    delegate_mode = ExtensionSettingModelTypeController::DelegateMode::
        kTransportModeWithSingleModel;
  }
  return std::make_unique<ExtensionSettingModelTypeController>(
      syncer::APP_SETTINGS, GetModelTypeStoreService()->GetStoreFactory(),
      extensions::settings_sync_util::GetSyncableServiceProvider(
          profile_, syncer::APP_SETTINGS),
      GetDumpStackClosure(), delegate_mode, profile_);
}

std::unique_ptr<syncer::ModelTypeController>
ChromeSyncClient::CreateWebAppsModelTypeController() {
  syncer::ModelTypeControllerDelegate* delegate =
      GetControllerDelegateForModelType(syncer::WEB_APPS).get();

  std::unique_ptr<syncer::ModelTypeControllerDelegate>
      delegate_for_transport_mode = nullptr;
  if (ShouldSyncAppsTypesInTransportMode()) {
    delegate_for_transport_mode =
        std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
            delegate);
  }
  return std::make_unique<syncer::ModelTypeController>(
      syncer::WEB_APPS,
      /*delegate_for_full_sync_mode=*/
      std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(delegate),
      /*delegate_for_transport_mode=*/
      std::move(delegate_for_transport_mode));
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

}  // namespace browser_sync
