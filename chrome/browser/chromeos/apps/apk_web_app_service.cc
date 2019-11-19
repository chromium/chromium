// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/apps/apk_web_app_service.h"

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/apps/apk_web_app_service_factory.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/web_applications/components/externally_installed_web_app_prefs.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/common/chrome_features.h"
#include "components/arc/mojom/app.mojom.h"
#include "components/arc/session/connection_holder.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/extension.h"
#include "url/gurl.h"

namespace {

// The pref dict is:
// {
//  ...
//  "web_app_apks" : {
//    <extension_id_1> : {
//      "package_name" : <apk_package_name_1>,
//      "should_remove": <bool>
//    },
//    <extension_id_2> : {
//      "package_name" : <apk_package_name_2>,
//      "should_remove": <bool>
//    },
//    ...
//  },
//  ...
// }
const char kWebAppToApkDictPref[] = "web_app_apks";
const char kPackageNameKey[] = "package_name";
const char kShouldRemoveKey[] = "should_remove";
constexpr char kLastAppId[] = "last_app_id";
constexpr char kPinIndex[] = "pin_index";

// Default icon size in pixels to request from ARC for an icon.
const int kDefaultIconSize = 192;

}  // namespace

namespace chromeos {

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
    : profile_(profile), arc_app_list_prefs_(ArcAppListPrefs::Get(profile)) {
  // Can be null in tests.
  if (arc_app_list_prefs_)
    arc_app_list_prefs_->AddObserver(this);

  observer_.Add(extensions::ExtensionRegistry::Get(profile));
}

ApkWebAppService::~ApkWebAppService() = default;

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
  if (!web_app::ExternallyInstalledWebAppPrefs::HasAppIdWithInstallSource(
          profile_->GetPrefs(), web_app_id,
          web_app::ExternalInstallSource::kArc)) {
    // Do not uninstall a web app that was not installed via ApkWebAppInstaller.
    return;
  }

  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile_);
  const extensions::Extension* extension = registry->GetExtensionById(
      web_app_id, extensions::ExtensionRegistry::EVERYTHING);
  if (extension) {
    extensions::ExtensionSystem::Get(profile_)
        ->extension_service()
        ->UninstallExtension(extension->id(), extensions::UNINSTALL_REASON_ARC,
                             /*error=*/nullptr);
  }
}

void ApkWebAppService::UpdateShelfPin(
    const arc::mojom::ArcPackageInfo* package_info) {
  std::string new_app_id;
  // Compute the current app id. It may have changed if the package has been
  // updated from an Android app to a web app, or vice versa.
  if (!package_info->web_app_info.is_null()) {
    new_app_id = web_app::GenerateAppIdFromURL(
        GURL(package_info->web_app_info->start_url));
  } else {
    // Get the first app in the package. If there are multiple apps in the
    // package there is no way to determine which app is more suitable to
    // replace the previous web app shortcut. For simplicity we will just use
    // the first one.
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
      auto* launcher_controller = ChromeLauncherController::instance();
      if (!launcher_controller)
        return;
      int index = launcher_controller->PinnedItemIndexByAppID(last_app_id);
      // The previously installed app has been uninstalled or hidden, in this
      // instance get the saved pin index and pin at that place.
      if (index == ChromeLauncherController::kInvalidIndex) {
        const base::Value* saved_index = arc_app_list_prefs_->GetPackagePrefs(
            package_info->package_name, kPinIndex);
        if (!(saved_index && saved_index->is_int()))
          return;
        launcher_controller->PinAppAtIndex(new_app_id, saved_index->GetInt());
        arc_app_list_prefs_->SetPackagePrefs(
            package_info->package_name, kPinIndex,
            base::Value(ChromeLauncherController::kInvalidIndex));
      } else {
        launcher_controller->ReplacePinnedItem(last_app_id, new_app_id);
      }
    }
  }
}

void ApkWebAppService::Shutdown() {
  // Can be null in tests.
  if (arc_app_list_prefs_) {
    arc_app_list_prefs_->RemoveObserver(this);
    arc_app_list_prefs_ = nullptr;
  }
}

void ApkWebAppService::OnPackageInstalled(
    const arc::mojom::ArcPackageInfo& package_info) {
  if (!base::FeatureList::IsEnabled(features::kApkWebAppInstalls))
    return;

  // This method is called when a) new packages are installed, and b) existing
  // packages are updated. In (b), there are two cases to handle: the package
  // could previously have been an Android app and has now become a web app, and
  // vice-versa.
  DictionaryPrefUpdate web_apps_to_apks(profile_->GetPrefs(),
                                        kWebAppToApkDictPref);

  // Search the pref dict for any |web_app_id| that has a value matching the
  // provided package name.
  std::string web_app_id;
  for (const auto& it : web_apps_to_apks->DictItems()) {
    const base::Value* v =
        it.second.FindKeyOfType(kPackageNameKey, base::Value::Type::STRING);

    if (v && (v->GetString() == package_info.package_name)) {
      web_app_id = it.first;
      break;
    }
  }

  bool was_previously_web_app = !web_app_id.empty();
  bool is_now_web_app = !package_info.web_app_info.is_null();

  // The previous and current states match. Nothing to do.
  if (is_now_web_app == was_previously_web_app)
    return;

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
      arc_app_list_prefs_->app_connection_holder(), RequestPackageIcon);
  if (!instance)
    return;

  instance->RequestPackageIcon(
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
  // launcher), which has called OnExtensionUninstalled() below and triggered
  // the uninstallation of the Android package.
  //
  // In this case, OnExtensionUninstalled() will have removed the associated
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
  // OnExtensionUninstalled() will do nothing.
  if (!base::FeatureList::IsEnabled(features::kApkWebAppInstalls))
    return;

  DictionaryPrefUpdate web_apps_to_apks(profile_->GetPrefs(),
                                        kWebAppToApkDictPref);

  // Search the pref dict for any |web_app_id| that has a value matching the
  // provided package name. We need to uninstall that |web_app_id|.
  std::string web_app_id;
  for (const auto& it : web_apps_to_apks->DictItems()) {
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
  if (!base::FeatureList::IsEnabled(features::kApkWebAppInstalls))
    return;

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

  for (const auto& it : web_apps_to_apks->DictItems()) {
    const base::Value* v =
        it.second.FindKeyOfType(kShouldRemoveKey, base::Value::Type::BOOLEAN);

    // If we don't need to uninstall the package, move along.
    if (!v || !v->GetBool())
      continue;

    // Without a package name, the dictionary isn't useful. Remove it.
    const std::string& web_app_id = it.first;
    v = it.second.FindKeyOfType(kPackageNameKey, base::Value::Type::STRING);
    if (!v) {
      web_apps_to_apks->RemoveKey(web_app_id);
      continue;
    }

    // Remove the web app id from prefs, otherwise the corresponding call to
    // OnPackageRemoved will start an uninstallation cycle. Take a copy of the
    // string otherwise deleting |v| will erase the object underling
    // a reference.
    std::string package_name = v->GetString();
    web_apps_to_apks->RemoveKey(web_app_id);
    instance->UninstallPackage(package_name);
  }
}

void ApkWebAppService::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UninstallReason reason) {
  if (!base::FeatureList::IsEnabled(features::kApkWebAppInstalls))
    return;

  DictionaryPrefUpdate web_apps_to_apks(profile_->GetPrefs(),
                                        kWebAppToApkDictPref);

  // Find the package name associated with the provided web app id.
  const base::Value* package_name_value = web_apps_to_apks->FindPathOfType(
      {extension->id(), kPackageNameKey}, base::Value::Type::STRING);
  const std::string package_name =
      package_name_value ? package_name_value->GetString() : "";

  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_app_list_prefs_->app_connection_holder(), UninstallPackage);

  if (package_name_value) {
    if (instance) {
      // Remove the web app id from prefs, otherwise the corresponding call to
      // OnPackageRemoved will start an uninstallation cycle.
      web_apps_to_apks->RemoveKey(extension->id());
      instance->UninstallPackage(package_name);
    } else {
      // Set that the app should be removed next time the ARC container is
      // ready.
      web_apps_to_apks->SetPath({extension->id(), kShouldRemoveKey},
                                base::Value(true));
    }
  }

  // Post task to make sure that all OnExtensionUninstalled observers get
  // fired before the callback called.
  if (web_app_uninstalled_callback_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(web_app_uninstalled_callback_),
                                  package_name, extension->id()));
  }
}

void ApkWebAppService::OnDidGetWebAppIcon(
    const std::string& package_name,
    arc::mojom::WebAppInfoPtr web_app_info,
    const std::vector<uint8_t>& icon_png_data) {
  ApkWebAppInstaller::Install(
      profile_, std::move(web_app_info), icon_png_data,
      base::BindOnce(&ApkWebAppService::OnDidFinishInstall,
                     weak_ptr_factory_.GetWeakPtr(), package_name),
      weak_ptr_factory_.GetWeakPtr());
}

void ApkWebAppService::OnDidFinishInstall(const std::string& package_name,
                                          const web_app::AppId& web_app_id,
                                          web_app::InstallResultCode code) {
  // Do nothing: any error cancels installation.
  if (code != web_app::InstallResultCode::kSuccessNewInstall)
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

  // For testing.
  if (web_app_installed_callback_)
    std::move(web_app_installed_callback_).Run(package_name, web_app_id);
}

}  // namespace chromeos
