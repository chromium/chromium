// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/apps/apk_web_app_service.h"

#include <map>
#include <utility>

#include "ash/components/arc/mojom/app.mojom.h"
#include "ash/components/arc/session/connection_holder.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/apps/apk_web_app_service_factory.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/web_app_service_ash.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "url/gurl.h"

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
const char kIsWebOnlyTwaKey[] = "is_web_only_twa";
const char kSha256FingerprintKey[] = "sha256_fingerprint";
constexpr char kLastAppId[] = "last_app_id";
constexpr char kPinIndex[] = "pin_index";
constexpr char kGeneratedWebApkPackagePrefix[] = "org.chromium.webapk.";

// Default icon size in pixels to request from ARC for an icon.
const int kDefaultIconSize = 192;

}  // namespace

namespace ash {

// static
ApkWebAppService* ApkWebAppService::Get(Profile* profile) {
  return ApkWebAppServiceFactory::GetForProfile(profile);
}

// static
void ApkWebAppService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(kWebAppToApkDictPref);
}

ApkWebAppService::ApkWebAppService(Profile* profile)
    : profile_(profile), arc_app_list_prefs_(nullptr) {
  DCHECK(web_app::AreWebAppsEnabled(profile));

  if (web_app::IsWebAppsCrosapiEnabled()) {
    apps::AppRegistryCache& app_registry_cache =
        apps::AppServiceProxyFactory::GetForProfile(profile)
            ->AppRegistryCache();
    app_registry_cache_observer_.Observe(&app_registry_cache);
  }

  // Can be null in tests.
  arc_app_list_prefs_ = ArcAppListPrefs::Get(profile);
  if (arc_app_list_prefs_)
    arc_app_list_prefs_observer_.Observe(arc_app_list_prefs_);

  provider_ = web_app::WebAppProvider::GetDeprecated(profile);
  DCHECK(provider_);
  if (!web_app::IsWebAppsCrosapiEnabled()) {
    install_manager_observer_.Observe(&provider_->install_manager());
  }
}

ApkWebAppService::~ApkWebAppService() = default;

bool ApkWebAppService::IsWebOnlyTwa(const web_app::AppId& app_id) {
  if (!web_app::IsWebAppsCrosapiEnabled() &&
      !IsWebAppInstalledFromArc(app_id)) {
    return false;
  }

  DictionaryPrefUpdate web_apps_to_apks(profile_->GetPrefs(),
                                        kWebAppToApkDictPref);

  // Find the entry associated with the provided web app id.
  const base::Value* v = web_apps_to_apks->FindPathOfType(
      {app_id, kIsWebOnlyTwaKey}, base::Value::Type::BOOLEAN);
  return v && v->GetBool();
}

bool ApkWebAppService::IsWebAppInstalledFromArc(
    const web_app::AppId& web_app_id) {
  if (web_app::IsWebAppsCrosapiEnabled()) {
    // The web app will only be in prefs under this key if it was installed from
    // ARC++.
    DictionaryPrefUpdate web_apps_to_apks(profile_->GetPrefs(),
                                          kWebAppToApkDictPref);
    const base::Value* v = web_apps_to_apks->FindKeyOfType(
        web_app_id, base::Value::Type::DICTIONARY);
    return v != nullptr;
  } else {
    web_app::WebAppRegistrar& registrar = provider_->registrar();
    const web_app::WebApp* app = registrar.GetAppById(web_app_id);
    return app ? app->IsWebAppStoreInstalledApp() : false;
  }
}

bool ApkWebAppService::IsWebAppShellPackage(const std::string& package_name) {
  DictionaryPrefUpdate web_apps_to_apks(profile_->GetPrefs(),
                                        kWebAppToApkDictPref);

  // Search the pref dict for any web app id that has a value matching the
  // provided package name.
  for (const auto it : web_apps_to_apks->DictItems()) {
    const base::Value* v =
        it.second.FindKeyOfType(kPackageNameKey, base::Value::Type::STRING);
    if (v && (v->GetString() == package_name))
      return true;
  }

  // If there is no associated web app id, the package name is not a
  // web app shell package.
  return false;
}

absl::optional<std::string> ApkWebAppService::GetPackageNameForWebApp(
    const web_app::AppId& app_id) {
  DictionaryPrefUpdate web_apps_to_apks(profile_->GetPrefs(),
                                        kWebAppToApkDictPref);

  // Find the entry associated with the provided web app id.
  const base::Value* v = web_apps_to_apks->FindPathOfType(
      {app_id, kPackageNameKey}, base::Value::Type::STRING);

  if (!v)
    return absl::nullopt;

  return absl::optional<std::string>(v->GetString());
}

absl::optional<std::string> ApkWebAppService::GetPackageNameForWebApp(
    const GURL& url) {
  web_app::WebAppRegistrar& registrar =
      web_app::WebAppProvider::GetDeprecated(profile_)->registrar();
  absl::optional<web_app::AppId> app_id = registrar.FindAppWithUrlInScope(url);
  if (!app_id)
    return absl::nullopt;

  return GetPackageNameForWebApp(app_id.value());
}

absl::optional<std::string> ApkWebAppService::GetCertificateSha256Fingerprint(
    const web_app::AppId& app_id) {
  if (!web_app::IsWebAppsCrosapiEnabled() &&
      !IsWebAppInstalledFromArc(app_id)) {
    return absl::nullopt;
  }

  DictionaryPrefUpdate web_apps_to_apks(profile_->GetPrefs(),
                                        kWebAppToApkDictPref);

  // Find the entry associated with the provided web app id.
  const base::Value* v = web_apps_to_apks->FindPathOfType(
      {app_id, kSha256FingerprintKey}, base::Value::Type::STRING);

  if (!v)
    return absl::nullopt;

  return absl::optional<std::string>(v->GetString());
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
  if (!web_app::IsWebAppsCrosapiEnabled() &&
      !IsWebAppInstalledFromArc(web_app_id)) {
    // Do not uninstall a web app that was not installed via ApkWebAppInstaller.
    return;
  }

  if (web_app::IsWebAppsCrosapiEnabled()) {
    crosapi::mojom::WebAppProviderBridge* web_app_provider_bridge =
        crosapi::CrosapiManager::Get()
            ->crosapi_ash()
            ->web_app_service_ash()
            ->GetWebAppProviderBridge();
    if (!web_app_provider_bridge) {
      // TODO(crbug.com/1225830): handle crosapi disconnections
      return;
    }
    web_app_provider_bridge->WebAppUninstalledInArc(web_app_id,
                                                    base::DoNothing());
  } else {
    DCHECK(provider_);
    provider_->install_finalizer().UninstallExternalWebApp(
        web_app_id, webapps::WebappUninstallSource::kArc, base::DoNothing());
  }
}

void ApkWebAppService::UpdateShelfPin(
    const arc::mojom::ArcPackageInfo* package_info) {
  std::string new_app_id;
  // Compute the current app id. It may have changed if the package has been
  // updated from an Android app to a web app, or vice versa.
  if (!package_info->web_app_info.is_null()) {
    new_app_id = web_app::GenerateAppId(
        /*manifest_id=*/absl::nullopt,
        GURL(package_info->web_app_info->start_url));
  } else {
    // Get the first app in the package. If there are multiple apps in the
    // package there is no way to determine which app is more suitable to
    // replace the previous web app shortcut. For simplicity we will just use
    // the first one.
    DCHECK(arc_app_list_prefs_);
    std::unordered_set<std::string> apps =
        arc_app_list_prefs_->GetAppsForPackage(package_info->package_name);
    if (!apps.empty())
      new_app_id = *apps.begin();
  }

  // Query for the old app id, which is cached in the package dict to ensure it
  // isn't overwritten before this method can run.
  const base::Value* last_app_id_value = arc_app_list_prefs_->GetPackagePrefs(
      package_info->package_name, kLastAppId);

  std::string last_app_id;
  if (last_app_id_value && last_app_id_value->is_string())
    last_app_id = last_app_id_value->GetString();

  if (new_app_id != last_app_id && !new_app_id.empty()) {
    arc_app_list_prefs_->SetPackagePrefs(package_info->package_name, kLastAppId,
                                         base::Value(new_app_id));
    if (!last_app_id.empty()) {
      auto* shelf_controller = ChromeShelfController::instance();
      if (!shelf_controller)
        return;
      int index = shelf_controller->PinnedItemIndexByAppID(last_app_id);
      // The previously installed app has been uninstalled or hidden, in this
      // instance get the saved pin index and pin at that place.
      if (index == ChromeShelfController::kInvalidIndex) {
        const base::Value* saved_index = arc_app_list_prefs_->GetPackagePrefs(
            package_info->package_name, kPinIndex);
        if (!(saved_index && saved_index->is_int()))
          return;
        shelf_controller->PinAppAtIndex(new_app_id, saved_index->GetInt());
        arc_app_list_prefs_->SetPackagePrefs(
            package_info->package_name, kPinIndex,
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
  DictionaryPrefUpdate web_apps_to_apks(profile_->GetPrefs(),
                                        kWebAppToApkDictPref);

  // Search the pref dict for any |web_app_id| that has a value matching the
  // provided package name.
  std::string web_app_id;
  for (const auto it : web_apps_to_apks->DictItems()) {
    const base::Value* v =
        it.second.FindKeyOfType(kPackageNameKey, base::Value::Type::STRING);

    if (v && (v->GetString() == package_info.package_name)) {
      web_app_id = it.first;
      break;
    }
  }

  bool was_previously_web_app = !web_app_id.empty();
  bool is_now_web_app = !package_info.web_app_info.is_null();

  // The previous and current states match.
  if (is_now_web_app == was_previously_web_app) {
    if (is_now_web_app && package_info.web_app_info->is_web_only_twa !=
                              IsWebOnlyTwa(web_app_id)) {
      UpdatePackageInfo(web_app_id, package_info.web_app_info);
    }

    return;
  }

  // Only call this function if there has been a state change from web app to
  // Android app or vice-versa.
  UpdateShelfPin(&package_info);

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
  DictionaryPrefUpdate web_apps_to_apks(profile_->GetPrefs(),
                                        kWebAppToApkDictPref);

  // Search the pref dict for any |web_app_id| that has a value matching the
  // provided package name. We need to uninstall that |web_app_id|.
  std::string web_app_id;
  for (const auto it : web_apps_to_apks->DictItems()) {
    const base::Value* v =
        it.second.FindKeyOfType(kPackageNameKey, base::Value::Type::STRING);

    if (v && (v->GetString() == package_name)) {
      web_app_id = it.first;
      break;
    }
  }

  if (web_app_id.empty())
    return;

  // Remove |web_app_id| so that we don't start an uninstallation loop.
  web_apps_to_apks->RemoveKey(web_app_id);
  UninstallWebApp(web_app_id);
}

void ApkWebAppService::OnPackageListInitialRefreshed() {
  // Scan through the list of apps to see if any were uninstalled while ARC
  // wasn't running.
  DictionaryPrefUpdate web_apps_to_apks(profile_->GetPrefs(),
                                        kWebAppToApkDictPref);

  // If ARC isn't unavailable, it's not going to become available since we're
  // occupying the UI thread. We'll try again later.
  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_app_list_prefs_->app_connection_holder(), UninstallPackage);
  if (!instance)
    return;

  std::map<std::string, std::string> app_ids_and_packages_to_remove;
  for (const auto it : web_apps_to_apks->DictItems()) {
    const base::Value* v =
        it.second.FindKeyOfType(kShouldRemoveKey, base::Value::Type::BOOLEAN);

    // If we don't need to uninstall the package, move along.
    if (!v || !v->GetBool())
      continue;

    // Without a package name, the dictionary isn't useful, so set it for
    // removal.
    const std::string& web_app_id = it.first;
    std::string package_name;
    v = it.second.FindKeyOfType(kPackageNameKey, base::Value::Type::STRING);
    if (v) {
      package_name = v->GetString();
    }

    app_ids_and_packages_to_remove.insert({web_app_id, package_name});
  }

  // Remove the web app id from prefs before uninstalling, otherwise the
  // corresponding call to OnPackageRemoved will start an uninstallation cycle.
  for (const auto& app_id_and_package_name : app_ids_and_packages_to_remove) {
    web_apps_to_apks->RemoveKey(app_id_and_package_name.first);
    const std::string& package_name = app_id_and_package_name.second;
    if (!package_name.empty())
      instance->UninstallPackage(package_name);
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

void ApkWebAppService::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  app_registry_cache_observer_.Reset();
}

void ApkWebAppService::MaybeRemoveArcPackageForWebApp(
    const web_app::AppId& web_app_id) {
  DictionaryPrefUpdate web_apps_to_apks(profile_->GetPrefs(),
                                        kWebAppToApkDictPref);

  // Find the package name associated with the provided web app id.
  const base::Value* package_name_value = web_apps_to_apks->FindPathOfType(
      {web_app_id, kPackageNameKey}, base::Value::Type::STRING);
  const std::string package_name =
      package_name_value ? package_name_value->GetString() : "";

  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_app_list_prefs_->app_connection_holder(), UninstallPackage);

  if (package_name_value) {
    if (instance) {
      // Remove the web app id from prefs, otherwise the corresponding call to
      // OnPackageRemoved will start an uninstallation cycle.
      web_apps_to_apks->RemoveKey(web_app_id);
      instance->UninstallPackage(package_name);
    } else {
      // Set that the app should be removed next time the ARC container is
      // ready.
      web_apps_to_apks->SetPath({web_app_id, kShouldRemoveKey},
                                base::Value(true));
    }
  }

  // Post task to make sure that all observers get fired before the callback
  // called.
  if (web_app_uninstalled_callback_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(web_app_uninstalled_callback_),
                                  package_name, web_app_id));
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
  // Do nothing: any error cancels installation.
  if (code != webapps::InstallResultCode::kSuccessNewInstall)
    return;

  // Set a pref to map |web_app_id| to |package_name| for future uninstallation.
  DictionaryPrefUpdate dict_update(profile_->GetPrefs(), kWebAppToApkDictPref);
  dict_update->SetPath({web_app_id, kPackageNameKey},
                       base::Value(package_name));

  // Set that the app should not be removed next time the ARC container starts
  // up. This is to ensure that web apps which are uninstalled in the browser
  // while the ARC container isn't running can be marked for uninstallation
  // when the container starts up again.
  dict_update->SetPath({web_app_id, kShouldRemoveKey}, base::Value(false));

  // Set a pref to indicate if the |web_app_id| is a web-only TWA.
  dict_update->SetPath({web_app_id, kIsWebOnlyTwaKey},
                       base::Value(is_web_only_twa));

  if (sha256_fingerprint.has_value()) {
    // Set a pref to hold the APK's certificate SHA256 fingerprint to use for
    // digital asset link verification.
    dict_update->SetPath({web_app_id, kSha256FingerprintKey},
                         base::Value(sha256_fingerprint.value()));
  }

  // For testing.
  if (web_app_installed_callback_)
    std::move(web_app_installed_callback_).Run(package_name, web_app_id);
}

void ApkWebAppService::UpdatePackageInfo(
    const std::string& app_id,
    const arc::mojom::WebAppInfoPtr& web_app_info) {
  DictionaryPrefUpdate dict_update(profile_->GetPrefs(), kWebAppToApkDictPref);
  dict_update->SetPath({app_id, kIsWebOnlyTwaKey},
                       base::Value(web_app_info->is_web_only_twa));
  dict_update->SetPath(
      {app_id, kSha256FingerprintKey},
      base::Value(web_app_info->certificate_sha256_fingerprint.value()));
}

}  // namespace ash
