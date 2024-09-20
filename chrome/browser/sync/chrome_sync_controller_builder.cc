// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/chrome_sync_controller_builder.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/security_events/security_event_recorder.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_syncable_service.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/channel_info.h"
#include "components/desks_storage/core/desk_sync_service.h"
#include "components/prefs/pref_service.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/model/data_type_controller_delegate.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/data_type_store_service.h"
#include "components/sync/model/forwarding_data_type_controller_delegate.h"
#include "components/sync/service/data_type_controller.h"
#include "components/sync/service/syncable_service_based_data_type_controller.h"
#include "content/public/browser/browser_thread.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/api/storage/settings_sync_util.h"
#include "chrome/browser/extensions/extension_sync_service.h"
#include "chrome/browser/sync/glue/extension_data_type_controller.h"
#include "chrome/browser/sync/glue/extension_setting_data_type_controller.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/components/arc/arc_util.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service.h"
#include "chrome/browser/ash/app_list/arc/arc_package_sync_data_type_controller.h"
#include "chrome/browser/ash/app_list/arc/arc_package_syncable_service.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/floating_sso/floating_sso_service.h"
#include "chrome/browser/ash/printing/oauth2/authorization_zones_manager.h"
#include "chrome/browser/ash/printing/printers_sync_bridge.h"
#include "chrome/browser/ash/printing/synced_printers_manager.h"
#include "chromeos/ash/components/sync_wifi/wifi_configuration_sync_service.h"
#include "components/sync_preferences/pref_service_syncable.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/webapk/webapk_sync_service.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace {

bool ShouldSyncBrowserTypes() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return crosapi::browser_util::IsAshBrowserSyncEnabled();
#else
  return true;
#endif
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
bool IsLacrosSecondaryProfile(Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  return !profile->IsMainProfile() &&
         !web_app::IsMainProfileCheckSkippedForTesting();
#else   // BUILDFLAG(IS_CHROMEOS_LACROS)
  return false;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
}

bool ShouldSyncAppsTypesInTransportMode() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // When apps sync controlled by Ash Sync settings, allow running apps-related
  // types (WEB_APPS, APPS and APP_SETTINGS) in transport-only mode using the
  // same `delegate`.
  return base::FeatureList::IsEnabled(syncer::kSyncChromeOSAppsToggleSharing);
#else   // BUILDFLAG(IS_CHROMEOS_LACROS)
  return false;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

}  // namespace

ChromeSyncControllerBuilder::ChromeSyncControllerBuilder() = default;

ChromeSyncControllerBuilder::~ChromeSyncControllerBuilder() = default;

void ChromeSyncControllerBuilder::SetDataTypeStoreService(
    syncer::DataTypeStoreService* data_type_store_service) {
  data_type_store_service_.Set(data_type_store_service);
}

void ChromeSyncControllerBuilder::SetSecurityEventRecorder(
    SecurityEventRecorder* security_event_recorder) {
  security_event_recorder_.Set(security_event_recorder);
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
void ChromeSyncControllerBuilder::SetExtensionSyncService(
    ExtensionSyncService* extension_sync_service) {
  extension_sync_service_.Set(extension_sync_service);
}

void ChromeSyncControllerBuilder::SetExtensionSystemProfile(Profile* profile) {
  extension_system_profile_.Set(profile);
}

void ChromeSyncControllerBuilder::SetThemeService(ThemeService* theme_service) {
  theme_service_.Set(theme_service);
}

void ChromeSyncControllerBuilder::SetWebAppProvider(
    web_app::WebAppProvider* web_app_provider) {
  web_app_provider_.Set(web_app_provider);
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(ENABLE_SPELLCHECK)
void ChromeSyncControllerBuilder::SetSpellcheckService(
    SpellcheckService* spellcheck_service) {
  spellcheck_service_.Set(spellcheck_service);
}
#endif  // BUILDFLAG(ENABLE_SPELLCHECK)

#if BUILDFLAG(IS_ANDROID)
void ChromeSyncControllerBuilder::SetWebApkSyncService(
    webapk::WebApkSyncService* web_apk_sync_service) {
  web_apk_sync_service_.Set(web_apk_sync_service);
}
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
void ChromeSyncControllerBuilder::SetAppListSyncableService(
    app_list::AppListSyncableService* app_list_syncable_service) {
  app_list_syncable_service_.Set(app_list_syncable_service);
}

void ChromeSyncControllerBuilder::SetAuthorizationZonesManager(
    ash::printing::oauth2::AuthorizationZonesManager*
        authorization_zones_manager) {
  authorization_zones_manager_.Set(authorization_zones_manager);
}

void ChromeSyncControllerBuilder::SetArcPackageSyncableService(
    arc::ArcPackageSyncableService* arc_package_syncable_service,
    Profile* arc_package_profile) {
  arc_package_syncable_service_.Set(arc_package_syncable_service);
  arc_package_profile_.Set(arc_package_profile);
}

void ChromeSyncControllerBuilder::SetDeskSyncService(
    desks_storage::DeskSyncService* desk_sync_service) {
  desk_sync_service_.Set(desk_sync_service);
}

void ChromeSyncControllerBuilder::SetFloatingSsoService(
    ash::floating_sso::FloatingSsoService* floating_sso_service) {
  floating_sso_service_.Set(floating_sso_service);
}

void ChromeSyncControllerBuilder::SetOsPrefServiceSyncable(
    sync_preferences::PrefServiceSyncable* os_pref_service_syncable) {
  os_pref_service_syncable_.Set(os_pref_service_syncable);
}

void ChromeSyncControllerBuilder::SetSyncedPrintersManager(
    ash::SyncedPrintersManager* synced_printer_manager) {
  synced_printer_manager_.Set(synced_printer_manager);
}

void ChromeSyncControllerBuilder::SetWifiConfigurationSyncService(
    ash::sync_wifi::WifiConfigurationSyncService*
        wifi_configuration_sync_service) {
  wifi_configuration_sync_service_.Set(wifi_configuration_sync_service);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

std::vector<std::unique_ptr<syncer::DataTypeController>>
ChromeSyncControllerBuilder::Build(syncer::SyncService* sync_service) {
  std::vector<std::unique_ptr<syncer::DataTypeController>> controllers;

  const base::RepeatingClosure dump_stack = base::BindRepeating(
      &syncer::ReportUnrecoverableError, chrome::GetChannel());

  syncer::RepeatingDataTypeStoreFactory data_type_store_factory =
      data_type_store_service_.value()->GetStoreFactory();

  if (ShouldSyncBrowserTypes()) {
    syncer::DataTypeControllerDelegate* security_events_delegate =
        security_event_recorder_.value()->GetControllerDelegate().get();
    // Forward both full-sync and transport-only modes to the same delegate,
    // since behavior for SECURITY_EVENTS does not differ.
    controllers.push_back(std::make_unique<syncer::DataTypeController>(
        syncer::SECURITY_EVENTS,
        /*delegate_for_full_sync_mode=*/
        std::make_unique<syncer::ForwardingDataTypeControllerDelegate>(
            security_events_delegate),
        /*delegate_for_transport_mode=*/
        std::make_unique<syncer::ForwardingDataTypeControllerDelegate>(
            security_events_delegate)));

#if BUILDFLAG(ENABLE_EXTENSIONS)
    if (extension_sync_service_.value()) {
      controllers.push_back(
          std::make_unique<browser_sync::ExtensionDataTypeController>(
              syncer::EXTENSIONS, data_type_store_factory,
              extension_sync_service_.value()->AsWeakPtr(), dump_stack,
              browser_sync::ExtensionDataTypeController::DelegateMode::
                  kLegacyFullSyncModeOnly,
              extension_system_profile_.value()));

      controllers.push_back(
          std::make_unique<browser_sync::ExtensionSettingDataTypeController>(
              syncer::EXTENSION_SETTINGS, data_type_store_factory,
              extensions::settings_sync_util::GetSyncableServiceProvider(
                  extension_system_profile_.value(),
                  syncer::EXTENSION_SETTINGS),
              dump_stack,
              browser_sync::ExtensionSettingDataTypeController::DelegateMode::
                  kLegacyFullSyncModeOnly,
              extension_system_profile_.value()));

      if (!IsLacrosSecondaryProfile(extension_system_profile_.value())) {
        auto delegate_mode = browser_sync::ExtensionDataTypeController::
            DelegateMode::kLegacyFullSyncModeOnly;
        auto setting_delegate_mode =
            browser_sync::ExtensionSettingDataTypeController::DelegateMode::
                kLegacyFullSyncModeOnly;
        if (ShouldSyncAppsTypesInTransportMode()) {
          delegate_mode = browser_sync::ExtensionDataTypeController::
              DelegateMode::kTransportModeWithSingleModel;
          setting_delegate_mode =
              browser_sync::ExtensionSettingDataTypeController::DelegateMode::
                  kTransportModeWithSingleModel;
        }

        controllers.push_back(
            std::make_unique<browser_sync::ExtensionDataTypeController>(
                syncer::APPS, data_type_store_factory,
                extension_sync_service_.value()->AsWeakPtr(), dump_stack,
                delegate_mode, extension_system_profile_.value()));

        controllers.push_back(
            std::make_unique<browser_sync::ExtensionSettingDataTypeController>(
                syncer::APP_SETTINGS, data_type_store_factory,
                extensions::settings_sync_util::GetSyncableServiceProvider(
                    extension_system_profile_.value(), syncer::APP_SETTINGS),
                dump_stack, setting_delegate_mode,
                extension_system_profile_.value()));
      }
    }

    if (theme_service_.value()) {
      controllers.push_back(
          std::make_unique<browser_sync::ExtensionDataTypeController>(
              syncer::THEMES, data_type_store_factory,
              theme_service_.value()->GetThemeSyncableService()->AsWeakPtr(),
              dump_stack,
              browser_sync::ExtensionDataTypeController::DelegateMode::
                  kLegacyFullSyncModeOnly,
              extension_system_profile_.value()));
    }

    if (!IsLacrosSecondaryProfile(extension_system_profile_.value()) &&
        web_app_provider_.value()) {
      syncer::DataTypeControllerDelegate* delegate =
          web_app_provider_.value()
              ->sync_bridge_unsafe()
              .change_processor()
              ->GetControllerDelegate()
              .get();

      std::unique_ptr<syncer::DataTypeControllerDelegate>
          delegate_for_transport_mode = nullptr;
      if (ShouldSyncAppsTypesInTransportMode()) {
        delegate_for_transport_mode =
            std::make_unique<syncer::ForwardingDataTypeControllerDelegate>(
                delegate);
      }
      controllers.push_back(std::make_unique<syncer::DataTypeController>(
          syncer::WEB_APPS,
          /*delegate_for_full_sync_mode=*/
          std::make_unique<syncer::ForwardingDataTypeControllerDelegate>(
              delegate),
          /*delegate_for_transport_mode=*/
          std::move(delegate_for_transport_mode)));
    }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(IS_ANDROID)
    if (web_apk_sync_service_.value()) {
      syncer::DataTypeControllerDelegate* delegate =
          web_apk_sync_service_.value()->GetDataTypeControllerDelegate().get();
      controllers.push_back(std::make_unique<syncer::DataTypeController>(
          syncer::WEB_APKS,
          /*delegate_for_full_sync_mode=*/
          std::make_unique<syncer::ForwardingDataTypeControllerDelegate>(
              delegate),
          /*delegate_for_transport_mode=*/
          std::make_unique<syncer::ForwardingDataTypeControllerDelegate>(
              delegate)));
    }
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_SPELLCHECK)
    // Chrome prefers OS provided spell checkers where they exist. So only sync
    // the custom dictionary on platforms that typically don't provide one.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
    // Dictionary sync is enabled by default.
    if (spellcheck_service_.value()) {
      controllers.push_back(
          std::make_unique<syncer::SyncableServiceBasedDataTypeController>(
              syncer::DICTIONARY, data_type_store_factory,
              spellcheck_service_.value()->GetCustomDictionary()->AsWeakPtr(),
              dump_stack,
              syncer::SyncableServiceBasedDataTypeController::DelegateMode::
                  kLegacyFullSyncModeOnly));
    }
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
#endif  // BUILDFLAG(ENABLE_SPELLCHECK)
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  CHECK(os_pref_service_syncable_.value());
  controllers.push_back(
      std::make_unique<syncer::SyncableServiceBasedDataTypeController>(
          syncer::OS_PREFERENCES, data_type_store_factory,
          os_pref_service_syncable_.value()
              ->GetSyncableService(syncer::OS_PREFERENCES)
              ->AsWeakPtr(),
          dump_stack,
          syncer::SyncableServiceBasedDataTypeController::DelegateMode::
              kTransportModeWithSingleModel));
  controllers.push_back(
      std::make_unique<syncer::SyncableServiceBasedDataTypeController>(
          syncer::OS_PRIORITY_PREFERENCES, data_type_store_factory,
          os_pref_service_syncable_.value()
              ->GetSyncableService(syncer::OS_PRIORITY_PREFERENCES)
              ->AsWeakPtr(),
          dump_stack,
          syncer::SyncableServiceBasedDataTypeController::DelegateMode::
              kTransportModeWithSingleModel));

  CHECK(synced_printer_manager_.value());
  controllers.push_back(std::make_unique<syncer::DataTypeController>(
      syncer::PRINTERS,
      std::make_unique<syncer::ForwardingDataTypeControllerDelegate>(
          synced_printer_manager_.value()
              ->GetSyncBridge()
              ->change_processor()
              ->GetControllerDelegate()
              .get()),
      /*delegate_for_transport_mode=*/nullptr));

  // Some profile types (e.g. sign-in screen) don't support app list.
  // Temporarily Disable AppListSyncableService for tablet form factor devices.
  // See crbug/1013732 for details.
  if (app_list_syncable_service_.value() &&
      !ash::switches::IsTabletFormFactor()) {
    // Runs in sync transport-mode and full-sync mode.
    controllers.push_back(
        std::make_unique<syncer::SyncableServiceBasedDataTypeController>(
            syncer::APP_LIST, data_type_store_factory,
            app_list_syncable_service_.value()->AsWeakPtr(), dump_stack,
            syncer::SyncableServiceBasedDataTypeController::DelegateMode::
                kTransportModeWithSingleModel));
  }

  if (arc_package_syncable_service_.value()) {
    controllers.push_back(std::make_unique<ArcPackageSyncDataTypeController>(
        data_type_store_factory,
        arc_package_syncable_service_.value()->AsWeakPtr(), dump_stack,
        sync_service, arc_package_profile_.value()));
  }

  if (wifi_configuration_sync_service_.value()) {
    syncer::DataTypeControllerDelegate* wifi_configurations_delegate =
        wifi_configuration_sync_service_.value()->GetControllerDelegate().get();
    controllers.push_back(std::make_unique<syncer::DataTypeController>(
        syncer::WIFI_CONFIGURATIONS,
        std::make_unique<syncer::ForwardingDataTypeControllerDelegate>(
            wifi_configurations_delegate),
        /*delegate_for_transport_mode=*/nullptr));
  }

  CHECK(desk_sync_service_.value());
  controllers.push_back(std::make_unique<syncer::DataTypeController>(
      syncer::WORKSPACE_DESK,
      std::make_unique<syncer::ForwardingDataTypeControllerDelegate>(
          desk_sync_service_.value()->GetControllerDelegate().get()),
      /*delegate_for_transport_mode=*/nullptr));

  if (authorization_zones_manager_.value()) {
    syncer::DataTypeControllerDelegate*
        printers_authorization_servers_delegate =
            authorization_zones_manager_.value()
                ->GetDataTypeSyncBridge()
                ->change_processor()
                ->GetControllerDelegate()
                .get();
    controllers.push_back(std::make_unique<syncer::DataTypeController>(
        syncer::PRINTERS_AUTHORIZATION_SERVERS,
        std::make_unique<syncer::ForwardingDataTypeControllerDelegate>(
            printers_authorization_servers_delegate),
        /*delegate_for_transport_mode=*/nullptr));
  }

  if (floating_sso_service_.value()) {
    controllers.push_back(std::make_unique<syncer::DataTypeController>(
        syncer::COOKIES,
        /*delegate_for_full_sync_mode=*/
        std::make_unique<syncer::ForwardingDataTypeControllerDelegate>(
            floating_sso_service_.value()->GetControllerDelegate().get()),
        /*delegate_for_transport_mode=*/nullptr));
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  return controllers;
}
