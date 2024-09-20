// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_CHROME_SYNC_CONTROLLER_BUILDER_H_
#define CHROME_BROWSER_SYNC_CHROME_SYNC_CONTROLLER_BUILDER_H_

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "extensions/buildflags/buildflags.h"

class Profile;
class SecurityEventRecorder;

namespace syncer {
class DataTypeController;
class DataTypeStoreService;
class SyncService;
}  // namespace syncer

namespace webapk {
class WebApkSyncService;
}  // namespace webapk

#if BUILDFLAG(ENABLE_EXTENSIONS)
class ExtensionSyncService;
class ThemeService;

namespace web_app {
class WebAppProvider;
}  // namespace web_app
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(ENABLE_SPELLCHECK)
class SpellcheckService;
#endif  // BUILDFLAG(ENABLE_SPELLCHECK)

#if BUILDFLAG(IS_CHROMEOS_ASH)
namespace app_list {
class AppListSyncableService;
}  // namespace app_list

namespace ash {
class SyncedPrintersManager;

namespace floating_sso {
class FloatingSsoService;
}  // namespace floating_sso

namespace printing::oauth2 {
class AuthorizationZonesManager;
}  // namespace printing::oauth2

namespace sync_wifi {
class WifiConfigurationSyncService;
}  // namespace sync_wifi
}  // namespace ash

namespace arc {
class ArcPackageSyncableService;
}  // namespace arc

namespace desks_storage {
class DeskSyncService;
}  // namespace desks_storage

namespace sync_preferences {
class PrefServiceSyncable;
}  // namespace sync_preferences
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Class responsible for instantiating sync controllers (DataTypeController)
// for datatypes / features under chrome/.
//
// NOTE: prefer adding new types to browser_sync::CommonControllerBuilder if the
// type is available in components/, even if it's not enabled on all embedders
// or platforms.
//
// Users of this class need to inject dependencies by invoking all setters (more
// on this below) and finally invoke `Build()` to instantiate controllers.
class ChromeSyncControllerBuilder {
 public:
  ChromeSyncControllerBuilder();
  ~ChromeSyncControllerBuilder();

  // Setters to inject dependencies. Each of these setters must be invoked
  // before invoking `Build()`. In some cases it is allowed to inject nullptr.
  void SetDataTypeStoreService(
      syncer::DataTypeStoreService* data_type_store_service);
  void SetSecurityEventRecorder(SecurityEventRecorder* security_event_recorder);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  void SetExtensionSyncService(ExtensionSyncService* extension_sync_service);
  void SetExtensionSystemProfile(Profile* profile);
  void SetThemeService(ThemeService* theme_service);
  void SetWebAppProvider(web_app::WebAppProvider* web_app_provider);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(ENABLE_SPELLCHECK)
  void SetSpellcheckService(SpellcheckService* spellcheck_service);
#endif  // BUILDFLAG(ENABLE_SPELLCHECK)

#if BUILDFLAG(IS_ANDROID)
  void SetWebApkSyncService(webapk::WebApkSyncService* web_apk_sync_service);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void SetAppListSyncableService(
      app_list::AppListSyncableService* app_list_syncable_service);
  void SetAuthorizationZonesManager(
      ash::printing::oauth2::AuthorizationZonesManager*
          authorization_zones_manager);
  void SetArcPackageSyncableService(
      arc::ArcPackageSyncableService* arc_package_syncable_service,
      Profile* arc_package_profile);
  void SetDeskSyncService(desks_storage::DeskSyncService* desk_sync_service);
  void SetFloatingSsoService(
      ash::floating_sso::FloatingSsoService* floating_sso_service);
  void SetOsPrefServiceSyncable(
      sync_preferences::PrefServiceSyncable* os_pref_service_syncable);
  void SetSyncedPrintersManager(
      ash::SyncedPrintersManager* synced_printer_manager);
  void SetWifiConfigurationSyncService(
      ash::sync_wifi::WifiConfigurationSyncService*
          wifi_configuration_sync_service);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Actually builds the controllers. All setters above must have been called
  // beforehand (null may or may not be allowed).
  std::vector<std::unique_ptr<syncer::DataTypeController>> Build(
      syncer::SyncService* sync_service);

 private:
  // Minimalistic fork of std::optional that enforces via CHECK that it has a
  // value when accessing it.
  template <typename Ptr>
  class SafeOptional {
   public:
    SafeOptional() = default;
    ~SafeOptional() = default;

    void Set(Ptr ptr) {
      CHECK(!ptr_.has_value());
      ptr_.emplace(std::move(ptr));
    }

    // Set() must have been called before.
    Ptr value() const {
      CHECK(ptr_.has_value());
      return ptr_.value();
    }

   private:
    std::optional<Ptr> ptr_;
  };

  // For all above, nullopt indicates the corresponding setter wasn't invoked.
  // nullptr indicates the setter was invoked with nullptr.
  SafeOptional<raw_ptr<syncer::DataTypeStoreService>> data_type_store_service_;
  SafeOptional<raw_ptr<SecurityEventRecorder>> security_event_recorder_;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  SafeOptional<raw_ptr<ExtensionSyncService>> extension_sync_service_;
  // This Profile instance has nothing special and is just the profile being
  // exercised by the factory. A more tailored name is used simply to limit its
  // usage beyond extensions.
  SafeOptional<raw_ptr<Profile>> extension_system_profile_;
  SafeOptional<raw_ptr<ThemeService>> theme_service_;
  SafeOptional<raw_ptr<web_app::WebAppProvider>> web_app_provider_;
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(ENABLE_SPELLCHECK)
  SafeOptional<raw_ptr<SpellcheckService>> spellcheck_service_;
#endif  // BUILDFLAG(ENABLE_SPELLCHECK)

#if BUILDFLAG(IS_ANDROID)
  SafeOptional<raw_ptr<webapk::WebApkSyncService>> web_apk_sync_service_;
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  SafeOptional<raw_ptr<app_list::AppListSyncableService>>
      app_list_syncable_service_;
  SafeOptional<raw_ptr<ash::printing::oauth2::AuthorizationZonesManager>>
      authorization_zones_manager_;
  SafeOptional<raw_ptr<arc::ArcPackageSyncableService>>
      arc_package_syncable_service_;
  // This Profile instance has nothing special and is just the profile being
  // exercised by the factory. A more tailored name is used simply to limit its
  // usage beyond ARC.
  SafeOptional<raw_ptr<Profile>> arc_package_profile_;
  SafeOptional<raw_ptr<desks_storage::DeskSyncService>> desk_sync_service_;
  SafeOptional<raw_ptr<ash::floating_sso::FloatingSsoService>>
      floating_sso_service_;
  SafeOptional<raw_ptr<sync_preferences::PrefServiceSyncable>>
      os_pref_service_syncable_;
  SafeOptional<raw_ptr<ash::SyncedPrintersManager>> synced_printer_manager_;
  SafeOptional<raw_ptr<ash::sync_wifi::WifiConfigurationSyncService>>
      wifi_configuration_sync_service_;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
};

#endif  // CHROME_BROWSER_SYNC_CHROME_SYNC_CONTROLLER_BUILDER_H_
