// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/apps/apk_web_app_service.h"

#include <map>
#include <utility>

#include "ash/components/arc/mojom/app.mojom.h"
#include "ash/components/arc/session/connection_holder.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/apps/apk_web_app_service_factory.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "url/gurl.h"

namespace ash {

namespace {

// The pref dict is:
// {
//  ...
//  "web_app_apks" : {
//    <web_app_id_1> : {
//      "package_name" : <apk_package_name_1>,
//      "should_remove": <bool>,
//      "is_web_only_twa": <bool>,
//      "sha256_fingerprint": <certificate_sha256_fingerprint_2> (optional)
//    },
//    <web_app_id_2> : {
//      "package_name" : <apk_package_name_2>,
//      "should_remove": <bool>,
//      "is_web_only_twa": <bool>,
//      "sha256_fingerprint": <certificate_sha256_fingerprint_2> (optional)
//    },
//    ...
//  },
//  ...
// }
const char kWebAppToApkDictPref[] = "web_app_apks";
const char kPackageNameKey[] = "package_name";
const char kShouldRemoveKey[] = "should_remove";

// TODO(crbug/1329727): remove these keys when
// |web_app::IsWebAppsCrosapiEnabled| is removed.
const char kIsWebOnlyTwaKey[] = "is_web_only_twa";
const char kSha256FingerprintKey[] = "sha256_fingerprint";

constexpr char kLastAppId[] = "last_app_id";
constexpr char kPinIndex[] = "pin_index";
constexpr char kGeneratedWebApkPackagePrefix[] = "org.chromium.webapk.";

// Default icon size in pixels to request from ARC for an icon.
const int kDefaultIconSize = 192;

bool IsAppInstalled(apps::AppRegistryCache& app_registry_cache,
                    const std::string& app_id) {
  bool installed = false;
  app_registry_cache.ForOneApp(
      app_id, [&installed](const apps::AppUpdate& update) {
        installed = apps_util::IsInstalled(update.Readiness());
      });
  return installed;
}

// Delegate implementation that actually talks to ARC And Lacros.
// It looks up |ArcAppListPrefs| in the profile to find the ARC connection.
class ApkWebAppServiceDelegateImpl : public ApkWebAppService::Delegate,
                                     public ApkWebAppInstaller::Owner {
 public:
  explicit ApkWebAppServiceDelegateImpl(Profile* profile)
      : profile_(profile), arc_app_list_prefs_(ArcAppListPrefs::Get(profile)) {
    DCHECK(arc_app_list_prefs_);
  }

  void MaybeInstallWebAppInLacros(const std::string& package_name,
                                  arc::mojom::WebAppInfoPtr web_app_info,
                                  WebAppInstallCallback callback) override {
    auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
        arc_app_list_prefs_->app_connection_holder(), GetPackageIcon);
    if (!instance) {
      return;
    }

    instance->GetPackageIcon(
        package_name, kDefaultIconSize, /*normalize=*/false,
        base::BindOnce(&ApkWebAppServiceDelegateImpl::OnDidGetWebAppIcon,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                       package_name, std::move(web_app_info)));
  }

  void MaybeUninstallWebAppInLacros(const web_app::AppId& web_app_id) override {
    if (crosapi::mojom::WebAppProviderBridge* web_app_provider_bridge =
            crosapi::CrosapiManager::Get()
                ->crosapi_ash()
                ->web_app_service_ash()
                ->GetWebAppProviderBridge()) {
      web_app_provider_bridge->WebAppUninstalledInArc(web_app_id,
                                                      base::DoNothing());
    }
  }

  void MaybeUninstallPackageInArc(const std::string& package_name) override {
    if (auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
            arc_app_list_prefs_->app_connection_holder(), UninstallPackage)) {
      instance->UninstallPackage(package_name);
    }
  }

 private:
  void OnDidGetWebAppIcon(WebAppInstallCallback callback,
                          const std::string& package_name,
                          arc::mojom::WebAppInfoPtr web_app_info,
                          arc::mojom::RawIconPngDataPtr icon) {
    ApkWebAppInstaller::Install(profile_, std::move(web_app_info),
                                std::move(icon), std::move(callback),
                                weak_ptr_factory_.GetWeakPtr());
  }

  raw_ptr<Profile> profile_;
  raw_ptr<ArcAppListPrefs> arc_app_list_prefs_;

  // Must go last.
  base::WeakPtrFactory<ApkWebAppServiceDelegateImpl> weak_ptr_factory_{this};
};

}  // namespace

ApkWebAppService::Delegate::~Delegate() = default;

// static
ApkWebAppService* ApkWebAppService::Get(Profile* profile) {
  return ApkWebAppServiceFactory::GetForProfile(profile);
}

// static
void ApkWebAppService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(kWebAppToApkDictPref);
}

ApkWebAppService::ApkWebAppService(Profile* profile, Delegate* test_delegate)
    : profile_(profile),
      arc_app_list_prefs_(nullptr),
      real_delegate_(std::make_unique<ApkWebAppServiceDelegateImpl>(profile)),
      test_delegate_(test_delegate) {
  DCHECK(web_app::AreWebAppsEnabled(profile));

  if (web_app::IsWebAppsCrosapiEnabled()) {
    apps::AppRegistryCache& app_registry_cache =
        apps::AppServiceProxyFactory::GetForProfile(profile)
            ->AppRegistryCache();
    app_registry_cache_observer_.Observe(&app_registry_cache);

    // null in unit tests
    if (auto* browser_manager = crosapi::BrowserManager::Get()) {
      keep_alive_ = browser_manager->KeepAlive(
          crosapi::BrowserManager::Feature::kApkWebAppService);
    }

    crosapi::WebAppServiceAsh* web_app_service_ash =
        crosapi::CrosapiManager::Get()->crosapi_ash()->web_app_service_ash();
    web_app_service_observer_.Observe(web_app_service_ash);
  }

  // Can be null in tests.
  arc_app_list_prefs_ = ArcAppListPrefs::Get(profile);
  if (arc_app_list_prefs_)
    arc_app_list_prefs_observer_.Observe(arc_app_list_prefs_);

  if (!web_app::IsWebAppsCrosapiEnabled()) {
    provider_ = web_app::WebAppProvider::GetForWebApps(profile);
    DCHECK(provider_);
    install_manager_observer_.Observe(&provider_->install_manager());
  }
}

ApkWebAppService::~ApkWebAppService() = default;

bool ApkWebAppService::IsWebOnlyTwa(const web_app::AppId& app_id) {
  if (web_app::IsWebAppsCrosapiEnabled()) {
    absl::optional<std::string> package_name = GetPackageNameForWebApp(app_id);
    if (!package_name) {
      return false;
    }
    std::unique_ptr<ArcAppListPrefs::PackageInfo> package =
        arc_app_list_prefs_->GetPackage(*package_name);
    if (!(package && package->web_app_info)) {
      return false;
    }
    return package->web_app_info->is_web_only_twa;
  }

  return IsWebOnlyTwaDeprecated(app_id);
}

bool ApkWebAppService::IsWebOnlyTwaDeprecated(const web_app::AppId& app_id) {
  if (!IsWebAppInstalledFromArc(app_id)) {
    return false;
  }

  if (const base::Value::Dict* app_dict = WebAppToApks().FindDict(app_id)) {
    return app_dict->FindBool(kIsWebOnlyTwaKey).value_or(false);
  }
  return false;
}

bool ApkWebAppService::IsWebAppInstalledFromArc(
    const web_app::AppId& web_app_id) {
  if (web_app::IsWebAppsCrosapiEnabled()) {
    // The web app will only be in prefs under this key if it was installed from
    // ARC++.
    return WebAppToApks().FindDict(web_app_id) != nullptr;
  } else {
    web_app::WebAppRegistrar& registrar = provider_->registrar_unsafe();
    const web_app::WebApp* app = registrar.GetAppById(web_app_id);
    return app ? app->IsWebAppStoreInstalledApp() : false;
  }
}

bool ApkWebAppService::IsWebAppShellPackage(const std::string& package_name) {
  // If there is no associated web app ID, the package name is not a web app
  // shell package.
  return GetWebAppIdForPackageName(package_name).has_value();
}

absl::optional<std::string> ApkWebAppService::GetPackageNameForWebApp(
    const web_app::AppId& app_id) {
  if (const base::Value::Dict* app_dict = WebAppToApks().FindDict(app_id)) {
    if (const std::string* value = app_dict->FindString(kPackageNameKey)) {
      return *value;
    }
  }
  return absl::nullopt;
}

absl::optional<std::string> ApkWebAppService::GetPackageNameForWebApp(
    const GURL& url) {
  auto* web_app_provider = web_app::WebAppProvider::GetForWebApps(profile_);
  if (!web_app_provider)
    return absl::nullopt;
  absl::optional<web_app::AppId> app_id =
      web_app_provider->registrar_unsafe().FindAppWithUrlInScope(url);
  if (!app_id)
    return absl::nullopt;

  return GetPackageNameForWebApp(app_id.value());
}

absl::optional<std::string> ApkWebAppService::GetWebAppIdForPackageName(
    const std::string& package_name) {
  for (auto [web_app_id, web_app_info_value] : WebAppToApks()) {
    const std::string* web_app_package_name =
        web_app_info_value.GetDict().FindString(kPackageNameKey);
    if (web_app_package_name && *web_app_package_name == package_name) {
      return web_app_id;
    }
  }
  return absl::nullopt;
}

absl::optional<std::string> ApkWebAppService::GetCertificateSha256Fingerprint(
    const web_app::AppId& app_id) {
  if (web_app::IsWebAppsCrosapiEnabled()) {
    absl::optional<std::string> package_name = GetPackageNameForWebApp(app_id);
    if (!package_name) {
      return absl::nullopt;
    }
    std::unique_ptr<ArcAppListPrefs::PackageInfo> package =
        arc_app_list_prefs_->GetPackage(*package_name);
    if (!(package && package->web_app_info)) {
      return absl::nullopt;
    }
    return package->web_app_info->certificate_sha256_fingerprint;
  }
  return GetCertificateSha256FingerprintDeprecated(app_id);
}

absl::optional<std::string>
ApkWebAppService::GetCertificateSha256FingerprintDeprecated(
    const web_app::AppId& app_id) {
  if (!IsWebAppInstalledFromArc(app_id)) {
    return absl::nullopt;
  }
  if (const base::Value::Dict* app_dict = WebAppToApks().FindDict(app_id)) {
    if (const std::string* value =
            app_dict->FindString(kSha256FingerprintKey)) {
      return *value;
    }
  }
  return absl::nullopt;
}

void ApkWebAppService::SetArcAppListPrefsForTesting(ArcAppListPrefs* prefs) {
  DCHECK(prefs);
  if (arc_app_list_prefs_)
    arc_app_list_prefs_->RemoveObserver(this);

  arc_app_list_prefs_ = prefs;
  arc_app_list_prefs_->AddObserver(this);
}

void ApkWebAppService::SetWebAppInstalledCallbackForTesting(
    WebAppCallbackForTesting web_app_installed_callback) {
  web_app_installed_callback_ = std::move(web_app_installed_callback);
}

void ApkWebAppService::SetWebAppUninstalledCallbackForTesting(
    WebAppCallbackForTesting web_app_uninstalled_callback) {
  web_app_uninstalled_callback_ = std::move(web_app_uninstalled_callback);
}

void ApkWebAppService::UninstallWebApp(const web_app::AppId& web_app_id) {
  DCHECK(!web_app::IsWebAppsCrosapiEnabled());
  if (!IsWebAppInstalledFromArc(web_app_id)) {
    // Do not uninstall a web app that was not installed via ApkWebAppInstaller.
    return;
  }

  DCHECK(provider_);
  provider_->install_finalizer().UninstallExternalWebApp(
      web_app_id, web_app::WebAppManagement::kWebAppStore,
      webapps::WebappUninstallSource::kArc, base::DoNothing());
}

void ApkWebAppService::UpdateShelfPin(
    const std::string& package_name,
    const arc::mojom::WebAppInfoPtr& web_app_info) {
  std::string new_app_id;
  // Compute the current app id. It may have changed if the package has been
  // updated from an Android app to a web app, or vice versa.
  if (!web_app_info.is_null()) {
    new_app_id = web_app::GenerateAppId(
        /*manifest_id=*/absl::nullopt, GURL(web_app_info->start_url));
  } else {
    // Get the first app in the package. If there are multiple apps in the
    // package there is no way to determine which app is more suitable to
    // replace the previous web app shortcut. For simplicity we will just use
    // the first one.
    DCHECK(arc_app_list_prefs_);
    std::unordered_set<std::string> apps =
        arc_app_list_prefs_->GetAppsForPackage(package_name);
    if (!apps.empty())
      new_app_id = *apps.begin();
  }

  // Query for the old app id, which is cached in the package dict to ensure it
  // isn't overwritten before this method can run.
  const base::Value* last_app_id_value =
      arc_app_list_prefs_->GetPackagePrefs(package_name, kLastAppId);

  std::string last_app_id;
  if (last_app_id_value && last_app_id_value->is_string())
    last_app_id = last_app_id_value->GetString();

  if (new_app_id != last_app_id && !new_app_id.empty()) {
    arc_app_list_prefs_->SetPackagePrefs(package_name, kLastAppId,
                                         base::Value(new_app_id));
    if (!last_app_id.empty()) {
      auto* shelf_controller = ChromeShelfController::instance();
      if (!shelf_controller)
        return;
      int index = shelf_controller->PinnedItemIndexByAppID(last_app_id);
      // The previously installed app has been uninstalled or hidden, in this
      // instance get the saved pin index and pin at that place.
      if (index == ChromeShelfController::kInvalidIndex) {
        const base::Value* saved_index =
            arc_app_list_prefs_->GetPackagePrefs(package_name, kPinIndex);
        if (!(saved_index && saved_index->is_int()))
          return;
        shelf_controller->PinAppAtIndex(new_app_id, saved_index->GetInt());
        arc_app_list_prefs_->SetPackagePrefs(
            package_name, kPinIndex,
            base::Value(ChromeShelfController::kInvalidIndex));
      } else {
        shelf_controller->ReplacePinnedItem(last_app_id, new_app_id);
      }
    }
  }
}

void ApkWebAppService::Shutdown() {
  // Can be null in tests.
  if (arc_app_list_prefs_)
    arc_app_list_prefs_ = nullptr;
}

void ApkWebAppService::OnPackageInstalled(
    const arc::mojom::ArcPackageInfo& package_info) {
  if (web_app::IsWebAppsCrosapiEnabled()) {
    SyncArcAndWebApps();
    return;
  }

  // Automatically generated WebAPKs have their lifecycle managed by
  // WebApkManager and do not need to be considered here.
  if (base::StartsWith(package_info.package_name,
                       kGeneratedWebApkPackagePrefix)) {
    return;
  }

  // This method is called when a) new packages are installed, and b) existing
  // packages are updated. In (b), there are two cases to handle: the package
  // could previously have been an Android app and has now become a web app, and
  // vice-versa.
  absl::optional<std::string> web_app_id =
      GetWebAppIdForPackageName(package_info.package_name);

  bool was_previously_web_app = web_app_id.has_value();
  bool is_now_web_app = !package_info.web_app_info.is_null();

  // The previous and current states match.
  if (is_now_web_app == was_previously_web_app) {
    if (is_now_web_app) {
      DCHECK(web_app_id);
      if (package_info.web_app_info->is_web_only_twa !=
          IsWebOnlyTwa(*web_app_id)) {
        UpdatePackageInfo(*web_app_id, package_info.web_app_info);
      }
    }
    return;
  }

  // Only call this function if there has been a state change from web app to
  // Android app or vice-versa.
  UpdateShelfPin(package_info.package_name, package_info.web_app_info);

  if (was_previously_web_app) {
    // The package was a web app, but now isn't. Remove the web app.
    OnPackageRemoved(package_info.package_name, true /* uninstalled */);
    return;
  }

  // The package is a web app but we don't have a corresponding browser-side
  // artifact. Install it.
  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_app_list_prefs_->app_connection_holder(), GetPackageIcon);
  if (!instance)
    return;

  instance->GetPackageIcon(
      package_info.package_name, kDefaultIconSize, /*normalize=*/false,
      base::BindOnce(&ApkWebAppService::OnDidGetWebAppIcon,
                     weak_ptr_factory_.GetWeakPtr(), package_info.package_name,
                     package_info.web_app_info.Clone()));
}

void ApkWebAppService::OnPackageRemoved(const std::string& package_name,
                                        bool uninstalled) {
  if (web_app::IsWebAppsCrosapiEnabled()) {
    absl::optional<std::string> web_app_id =
        GetWebAppIdForPackageName(package_name);
    if (web_app_id) {
      const base::Value::Dict* app_dict = WebAppToApks().FindDict(*web_app_id);
      if (app_dict && app_dict->FindBool(kShouldRemoveKey).value_or(false)) {
        // This package removal was triggered by web app removal, so cleanup and
        // do not kick off the uninstallation loop again.
        ScopedDictPrefUpdate(profile_->GetPrefs(), kWebAppToApkDictPref)
            ->Remove(*web_app_id);
      } else {
        // Package was removed by the user in ARC.
        SyncArcAndWebApps();
      }
    }
    return;
  }

  // Called when an Android package is uninstalled. The package may be
  // associated with an installed web app. If it is, there are 2 potential
  // cases:
  // 1) The user has uninstalled the web app already (e.g. via the
  // launcher), which has called OnWebAppWillBeUninstalled() below and triggered
  // the uninstallation of the Android package.
  //
  // In this case, OnWebAppWillBeUninstalled() will have removed the associated
  // web_app_id from the pref dict before triggering uninstallation, so this
  // method will do nothing.
  //
  // 2) The user has uninstalled the Android package in ARC (e.g. via the Play
  // Store app).
  //
  // In this case, the web app is *not yet* uninstalled when this method is
  // called, so the associated web_app_id is in the pref dict, and this method
  // will trigger the uninstallation of the web app. Similarly, this method
  // removes the associated web_app_id before triggering uninstallation, so
  // OnWebAppWillBeUninstalled() will do nothing.

  // Search the pref dict for any |web_app_id| that has a value matching the
  // provided package name. We need to uninstall that |web_app_id|.
  absl::optional<std::string> web_app_id =
      GetWebAppIdForPackageName(package_name);
  if (!web_app_id) {
    return;
  }

  // Remove |web_app_id| so that we don't start an uninstallation loop.
  ScopedDictPrefUpdate web_apps_to_apks(profile_->GetPrefs(),
                                        kWebAppToApkDictPref);
  web_apps_to_apks->Remove(*web_app_id);
  UninstallWebApp(*web_app_id);
}

void ApkWebAppService::OnPackageListInitialRefreshed() {
  if (web_app::IsWebAppsCrosapiEnabled()) {
    SyncArcAndWebApps();
    return;
  }

  // Scan through the list of apps to see if any were uninstalled while ARC
  // wasn't running.

  // If ARC isn't unavailable, it's not going to become available since we're
  // occupying the UI thread. We'll try again later.
  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_app_list_prefs_->app_connection_holder(), UninstallPackage);
  if (!instance)
    return;

  std::map<std::string, std::string> app_ids_and_packages_to_remove;
  for (const auto [web_app_id, web_app_info_value] : WebAppToApks()) {
    // If we don't need to uninstall the package, move along.
    if (!web_app_info_value.GetDict()
             .FindBool(kShouldRemoveKey)
             .value_or(false)) {
      continue;
    }

    if (const std::string* package_name =
            web_app_info_value.GetDict().FindString(kPackageNameKey)) {
      app_ids_and_packages_to_remove.insert({web_app_id, *package_name});
    } else {
      // Without a package name, the dictionary isn't useful, so drop it from
      // prefs without uninstalling.
      app_ids_and_packages_to_remove.insert({web_app_id, ""});
    }
  }

  // Remove the web app id from prefs before uninstalling, otherwise the
  // corresponding call to OnPackageRemoved will start an uninstallation cycle.
  ScopedDictPrefUpdate web_apps_to_apks(profile_->GetPrefs(),
                                        kWebAppToApkDictPref);
  for (const auto& app_id_and_package_name : app_ids_and_packages_to_remove) {
    web_apps_to_apks->Remove(app_id_and_package_name.first);
    const std::string& package_name = app_id_and_package_name.second;
    if (!package_name.empty()) {
      instance->UninstallPackage(package_name);
    }
  }
}

void ApkWebAppService::OnArcAppListPrefsDestroyed() {
  arc_app_list_prefs_observer_.Reset();
}

void ApkWebAppService::OnWebAppWillBeUninstalled(
    const web_app::AppId& web_app_id) {
  MaybeRemoveArcPackageForWebApp(web_app_id);
}

void ApkWebAppService::OnWebAppInstallManagerDestroyed() {
  install_manager_observer_.Reset();
}

void ApkWebAppService::OnAppUpdate(const apps::AppUpdate& update) {
  if (update.AppType() == apps::AppType::kWeb &&
      update.Readiness() == apps::Readiness::kUninstalledByUser) {
    MaybeRemoveArcPackageForWebApp(update.AppId());
  }
}

void ApkWebAppService::OnAppTypeInitialized(apps::AppType app_type) {
  if (app_type == apps::AppType::kWeb) {
    // Web apps are published, try syncing.
    SyncArcAndWebApps();
  }
}

void ApkWebAppService::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  app_registry_cache_observer_.Reset();
}

void ApkWebAppService::OnWebAppProviderBridgeConnected() {
  SyncArcAndWebApps();
}

void ApkWebAppService::MaybeRemoveArcPackageForWebApp(
    const web_app::AppId& web_app_id) {
  absl::optional<std::string> package_name =
      GetPackageNameForWebApp(web_app_id);
  if (package_name) {
    ScopedDictPrefUpdate web_apps_to_apks(profile_->GetPrefs(),
                                          kWebAppToApkDictPref);
    if (web_app::IsWebAppsCrosapiEnabled()) {
      std::unique_ptr<ArcAppListPrefs::PackageInfo> package =
          arc_app_list_prefs_->GetPackage(*package_name);
      if (package && package->web_app_info) {
        // Mark for removal and kick off the sync.
        web_apps_to_apks->EnsureDict(web_app_id)->Set(kShouldRemoveKey, true);
        SyncArcAndWebApps();
      } else {
        // 1) ARC package was already removed and triggered web app
        //    uninstallation, so there is nothing to remove.
        // 2) ARC package is no longer a web app.
        //
        // In either case we clean up the prefs and finish.
        web_apps_to_apks->Remove(web_app_id);
      }
    } else {
      if (auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
              arc_app_list_prefs_->app_connection_holder(), UninstallPackage)) {
        // Remove the web app id from prefs, otherwise the corresponding call to
        // OnPackageRemoved will start an uninstallation cycle.
        web_apps_to_apks->Remove(web_app_id);
        instance->UninstallPackage(*package_name);
      } else {
        // Set that the app should be removed next time the ARC container is
        // ready.
        web_apps_to_apks->EnsureDict(web_app_id)->Set(kShouldRemoveKey, true);
      }
    }
  }

  // Post task to make sure that all observers get fired before the callback
  // called.
  if (web_app_uninstalled_callback_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(web_app_uninstalled_callback_),
                                  package_name.value_or(""), web_app_id));
  }
}

void ApkWebAppService::OnDidGetWebAppIcon(
    const std::string& package_name,
    arc::mojom::WebAppInfoPtr web_app_info,
    arc::mojom::RawIconPngDataPtr icon) {
  ApkWebAppInstaller::Install(
      profile_, std::move(web_app_info), std::move(icon),
      base::BindOnce(&ApkWebAppService::OnDidFinishInstall,
                     weak_ptr_factory_.GetWeakPtr(), package_name),
      weak_ptr_factory_.GetWeakPtr());
}

void ApkWebAppService::OnDidFinishInstall(
    const std::string& package_name,
    const web_app::AppId& web_app_id,
    bool is_web_only_twa,
    const absl::optional<std::string> sha256_fingerprint,
    webapps::InstallResultCode code) {
  bool success = false;
  if (web_app::IsWebAppsCrosapiEnabled()) {
    success = webapps::IsSuccess(code);
  } else {
    success = code == webapps::InstallResultCode::kSuccessNewInstall;
  }

  if (success) {
    // Set a pref to map |web_app_id| to |package_name| for future
    // uninstallation.
    ScopedDictPrefUpdate dict_update(profile_->GetPrefs(),
                                     kWebAppToApkDictPref);
    base::Value::Dict* web_app_dict = dict_update->EnsureDict(web_app_id);
    web_app_dict->Set(kPackageNameKey, package_name);

    // Set that the app should not be removed next time the ARC container starts
    // up. This is to ensure that web apps which are uninstalled in the browser
    // while the ARC container isn't running can be marked for uninstallation
    // when the container starts up again.
    web_app_dict->Set(kShouldRemoveKey, false);

    // TODO(crbug/1329727): remove these keys when
    // |web_app::IsWebAppsCrosapiEnabled| is removed.

    // Set a pref to indicate if the |web_app_id| is a web-only TWA.
    web_app_dict->Set(kIsWebOnlyTwaKey, is_web_only_twa);

    if (sha256_fingerprint.has_value()) {
      // Set a pref to hold the APK's certificate SHA256 fingerprint to use for
      // digital asset link verification.
      web_app_dict->Set(kSha256FingerprintKey, sha256_fingerprint.value());
    }
  }

  // For testing.
  if (web_app_installed_callback_)
    std::move(web_app_installed_callback_).Run(package_name, web_app_id);
}

void ApkWebAppService::UpdatePackageInfo(
    const std::string& app_id,
    const arc::mojom::WebAppInfoPtr& web_app_info) {
  ScopedDictPrefUpdate dict_update(profile_->GetPrefs(), kWebAppToApkDictPref);
  base::Value::Dict* app_id_dict = dict_update->EnsureDict(app_id);
  app_id_dict->Set(kIsWebOnlyTwaKey, web_app_info->is_web_only_twa);
  app_id_dict->Set(kSha256FingerprintKey,
                   web_app_info->certificate_sha256_fingerprint.value());
}

const base::Value::Dict& ApkWebAppService::WebAppToApks() const {
  const base::Value::Dict& value =
      profile_->GetPrefs()->GetDict(kWebAppToApkDictPref);
  return value;
}

void ApkWebAppService::SyncArcAndWebApps() {
  DCHECK(web_app::IsWebAppsCrosapiEnabled());

  // Check that we have the initial state of both ARC packages and installed web
  // apps before attempting to reconcile installation state.
  apps::AppRegistryCache& app_registry_cache =
      apps::AppServiceProxyFactory::GetForProfile(profile_)->AppRegistryCache();
  if (!app_registry_cache.IsAppTypeInitialized(apps::AppType::kWeb)) {
    return;
  }
  if (!arc_app_list_prefs_->package_list_initial_refreshed()) {
    return;
  }

  std::vector<std::string> remove_from_prefs;
  for (const auto [web_app_id, web_app_info_value] : WebAppToApks()) {
    auto& web_app_info_dict = web_app_info_value.GetDict();
    const std::string* package_name =
        web_app_info_dict.FindString(kPackageNameKey);
    DCHECK(package_name);
    if (!package_name ||
        base::StartsWith(*package_name, kGeneratedWebApkPackagePrefix)) {
      // This shouldn't happen, but clean up bad data anyway.
      remove_from_prefs.push_back(web_app_id);
      continue;
    }
    if (!IsAppInstalled(app_registry_cache, web_app_id) &&
        !web_app_info_dict.FindBool(kShouldRemoveKey).value_or(false)) {
      // If the entry is for a non-existent web app AND isn't a marker for ARC
      // package uninstallation, it's stale (possibly due to a crash before web
      // app uninstallation callback is processed), so just remove it.
      remove_from_prefs.push_back(web_app_id);
    }
  }

  for (const auto& p : remove_from_prefs) {
    ScopedDictPrefUpdate(profile_->GetPrefs(), kWebAppToApkDictPref)->Remove(p);
  }

  // Collect currently installed ARC packages.
  std::map<std::string, std::unique_ptr<ArcAppListPrefs::PackageInfo>>
      arc_packages;
  for (const std::string& package_name :
       arc_app_list_prefs_->GetPackagesFromPrefs()) {
    // Automatically generated WebAPKs have their lifecycle managed by
    // WebApkManager and do not need to be considered here.
    if (base::StartsWith(package_name, kGeneratedWebApkPackagePrefix)) {
      continue;
    }
    arc_packages[package_name] = arc_app_list_prefs_->GetPackage(package_name);
  }

  // For each ARC package, decide if a matching web app needs to be installed or
  // uninstalled, if an ARC package becomes a non-web-app package.
  for (const auto& [package_name, package] : arc_packages) {
    absl::optional<std::string> web_app_id =
        GetWebAppIdForPackageName(package_name);

    bool was_web_app = web_app_id.has_value();
    bool is_web_app = !package->web_app_info.is_null();

    if (!was_web_app && is_web_app) {
      UpdateShelfPin(package_name, package->web_app_info);
      // The package is a web app but we don't have a corresponding browser-side
      // artifact. Install it.
      GetDelegate().MaybeInstallWebAppInLacros(
          package->package_name, std::move(package->web_app_info),
          base::BindOnce(&ApkWebAppService::OnDidFinishInstall,
                         weak_ptr_factory_.GetWeakPtr(),
                         package->package_name));
    } else if (was_web_app && !is_web_app) {
      UpdateShelfPin(package_name, package->web_app_info);
      // The package was a web app, but now isn't. Remove the web app.
      DCHECK(web_app_id);
      GetDelegate().MaybeUninstallWebAppInLacros(*web_app_id);
    } else if (was_web_app && is_web_app) {
      // The previous and current states match AND the package is a web app.
      // TODO(crbug/1329727): remove this block when
      // |web_app::IsWebAppsCrosapiEnabled| is removed.
      DCHECK(web_app_id);
      if ((package->web_app_info->is_web_only_twa !=
           IsWebOnlyTwaDeprecated(*web_app_id)) ||
          (package->web_app_info->certificate_sha256_fingerprint !=
           GetCertificateSha256FingerprintDeprecated(*web_app_id))) {
        UpdatePackageInfo(*web_app_id, package->web_app_info);
      }
    }
  }

  // For each web app entry, check if needs to be uninstalled, or matching ARC
  // package needs to be uninstalled.
  std::vector<std::string> arc_apps_to_uninstall;
  std::vector<std::string> web_apps_to_uninstall;
  for (const auto [web_app_id, web_app_info_value] : WebAppToApks()) {
    auto& web_app_info_dict = web_app_info_value.GetDict();
    const std::string* package_name =
        web_app_info_dict.FindString(kPackageNameKey);
    DCHECK(package_name);
    if (!package_name) {
      // This shouldn't happen, but ignore bad data anyway.
      continue;
    }
    if (base::Contains(arc_packages, *package_name)) {
      if (web_app_info_dict.FindBool(kShouldRemoveKey).value_or(false)) {
        // ARC app should be uninstalled.
        arc_apps_to_uninstall.push_back(*package_name);
      }
    } else {
      // Web app should be uninstalled.
      web_apps_to_uninstall.push_back(web_app_id);
    }
  }
  for (const std::string& package_name : arc_apps_to_uninstall) {
    GetDelegate().MaybeUninstallPackageInArc(package_name);
  }
  for (const std::string& web_app_id : web_apps_to_uninstall) {
    GetDelegate().MaybeUninstallWebAppInLacros(web_app_id);
  }
}

}  // namespace ash
