// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_service_proxy_lacros.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/containers/contains.h"
#include "base/debug/dump_without_crashing.h"
#include "base/location.h"
#include "base/notreached.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_source.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/browser_app_instance_forwarder.h"
#include "chrome/browser/apps/app_service/browser_app_instance_tracker.h"
#include "chrome/browser/apps/app_service/intent_util.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/apps/app_service/publishers/extension_apps.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/app_service/lacros_web_apps_controller.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/services/app_service/app_service_mojom_impl.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "content/public/browser/url_data_source.h"
#include "ui/display/types/display_constants.h"
#include "url/url_constants.h"

namespace apps {

AppServiceProxyLacros::AppServiceProxyLacros(Profile* profile)
    : inner_icon_loader_(this),
      icon_coalescer_(&inner_icon_loader_),
      outer_icon_loader_(&icon_coalescer_,
                         apps::IconCache::GarbageCollectionPolicy::kEager),
      profile_(profile) {
  if (web_app::IsWebAppsCrosapiEnabled()) {
    auto* service = chromeos::LacrosService::Get();
    if (service &&
        service->IsAvailable<crosapi::mojom::BrowserAppInstanceRegistry>()) {
      browser_app_instance_tracker_ =
          std::make_unique<apps::BrowserAppInstanceTracker>(
              profile_, app_registry_cache_);
      auto& registry =
          service->GetRemote<crosapi::mojom::BrowserAppInstanceRegistry>();
      DCHECK(registry);
      browser_app_instance_forwarder_ =
          std::make_unique<apps::BrowserAppInstanceForwarder>(
              *browser_app_instance_tracker_, registry);
    }
  }
}

AppServiceProxyLacros::~AppServiceProxyLacros() = default;

void AppServiceProxyLacros::ReInitializeForTesting(Profile* profile) {
  // Some test code creates a profile and profile-linked services, like the App
  // Service, before the profile is fully initialized. Such tests can call this
  // after full profile initialization to ensure the App Service implementation
  // has all of profile state it needs.
  crosapi_receiver_.reset();
  profile_ = profile;
  is_using_testing_profile_ = true;
  Initialize();
}

apps::AppRegistryCache& AppServiceProxyLacros::AppRegistryCache() {
  return app_registry_cache_;
}

apps::AppCapabilityAccessCache&
AppServiceProxyLacros::AppCapabilityAccessCache() {
  return app_capability_access_cache_;
}

BrowserAppLauncher* AppServiceProxyLacros::BrowserAppLauncher() {
  return browser_app_launcher_.get();
}

apps::PreferredAppsListHandle& AppServiceProxyLacros::PreferredAppsList() {
  return preferred_apps_list_;
}

apps::BrowserAppInstanceTracker*
AppServiceProxyLacros::BrowserAppInstanceTracker() {
  return browser_app_instance_tracker_.get();
}

absl::optional<IconKey> AppServiceProxyLacros::GetIconKey(
    const std::string& app_id) {
  return outer_icon_loader_.GetIconKey(app_id);
}

std::unique_ptr<apps::IconLoader::Releaser>
AppServiceProxyLacros::LoadIconFromIconKey(AppType app_type,
                                           const std::string& app_id,
                                           const IconKey& icon_key,
                                           IconType icon_type,
                                           int32_t size_hint_in_dip,
                                           bool allow_placeholder_icon,
                                           apps::LoadIconCallback callback) {
  return outer_icon_loader_.LoadIconFromIconKey(
      app_type, app_id, icon_key, icon_type, size_hint_in_dip,
      allow_placeholder_icon, std::move(callback));
}

std::unique_ptr<apps::IconLoader::Releaser>
AppServiceProxyLacros::LoadIconFromIconKey(
    apps::mojom::AppType app_type,
    const std::string& app_id,
    apps::mojom::IconKeyPtr icon_key,
    apps::mojom::IconType icon_type,
    int32_t size_hint_in_dip,
    bool allow_placeholder_icon,
    apps::mojom::Publisher::LoadIconCallback callback) {
  return outer_icon_loader_.LoadIconFromIconKey(
      app_type, app_id, std::move(icon_key), icon_type, size_hint_in_dip,
      allow_placeholder_icon, std::move(callback));
}

void AppServiceProxyLacros::Launch(const std::string& app_id,
                                   int32_t event_flags,
                                   apps::mojom::LaunchSource launch_source,
                                   apps::mojom::WindowInfoPtr window_info) {
  if (!remote_crosapi_app_service_proxy_) {
    return;
  }

  if (crosapi_app_service_proxy_version_ <
      int{crosapi::mojom::AppServiceProxy::MethodMinVersions::
              kLaunchMinVersion}) {
    LOG(WARNING) << "Ash AppServiceProxy version "
                 << crosapi_app_service_proxy_version_
                 << " does not support Launch().";
    return;
  }

  remote_crosapi_app_service_proxy_->Launch(
      CreateCrosapiLaunchParamsWithEventFlags(this, app_id, event_flags,
                                              launch_source,
                                              display::kInvalidDisplayId));
}

void AppServiceProxyLacros::LaunchAppWithFiles(
    const std::string& app_id,
    int32_t event_flags,
    apps::mojom::LaunchSource launch_source,
    apps::mojom::FilePathsPtr file_paths) {
  if (!remote_crosapi_app_service_proxy_) {
    return;
  }

  if (crosapi_app_service_proxy_version_ <
      int{crosapi::mojom::AppServiceProxy::MethodMinVersions::
              kLaunchMinVersion}) {
    LOG(WARNING) << "Ash AppServiceProxy version "
                 << crosapi_app_service_proxy_version_
                 << " does not support Launch().";
    return;
  }
  auto params = CreateCrosapiLaunchParamsWithEventFlags(
      this, app_id, event_flags, launch_source, display::kInvalidDisplayId);
  params->intent = apps_util::CreateCrosapiIntentForViewFiles(file_paths);
  remote_crosapi_app_service_proxy_->Launch(std::move(params));
}

void AppServiceProxyLacros::LaunchAppWithIntent(
    const std::string& app_id,
    int32_t event_flags,
    apps::mojom::IntentPtr intent,
    apps::mojom::LaunchSource launch_source,
    apps::mojom::WindowInfoPtr window_info) {
  CHECK(intent);

  if (!remote_crosapi_app_service_proxy_) {
    return;
  }

  if (crosapi_app_service_proxy_version_ <
      int{crosapi::mojom::AppServiceProxy::MethodMinVersions::
              kLaunchMinVersion}) {
    LOG(WARNING) << "Ash AppServiceProxy version "
                 << crosapi_app_service_proxy_version_
                 << " does not support Launch().";
    return;
  }

  auto params = CreateCrosapiLaunchParamsWithEventFlags(
      this, app_id, event_flags, launch_source,
      window_info ? window_info->display_id : display::kInvalidDisplayId);
  params->intent =
      apps_util::ConvertAppServiceToCrosapiIntent(intent, profile_);
  remote_crosapi_app_service_proxy_->Launch(std::move(params));
}

void AppServiceProxyLacros::LaunchAppWithUrl(
    const std::string& app_id,
    int32_t event_flags,
    GURL url,
    apps::mojom::LaunchSource launch_source,
    apps::mojom::WindowInfoPtr window_info) {
  LaunchAppWithIntent(app_id, event_flags, apps_util::CreateIntentFromUrl(url),
                      launch_source, std::move(window_info));
}

void AppServiceProxyLacros::LaunchAppWithParams(AppLaunchParams&& params,
                                                LaunchCallback callback) {
  if (!remote_crosapi_app_service_proxy_) {
    return;
  }

  if (crosapi_app_service_proxy_version_ <
      int{crosapi::mojom::AppServiceProxy::MethodMinVersions::
              kLaunchMinVersion}) {
    LOG(WARNING) << "Ash AppServiceProxy version "
                 << crosapi_app_service_proxy_version_
                 << " does not support Launch().";
    return;
  }

  remote_crosapi_app_service_proxy_->Launch(
      ConvertLaunchParamsToCrosapi(params, profile_));

  // TODO(crbug.com/1244506): Add params on crosapi and implement this.
  std::move(callback).Run(LaunchResult());
}

void AppServiceProxyLacros::SetPermission(
    const std::string& app_id,
    apps::mojom::PermissionPtr permission) {
  NOTIMPLEMENTED();
}

void AppServiceProxyLacros::Uninstall(
    const std::string& app_id,
    apps::mojom::UninstallSource uninstall_source,
    gfx::NativeWindow parent_window) {
  // On non-ChromeOS, publishers run the remove dialog.
  auto app_type = app_registry_cache_.GetAppType(app_id);
  if (app_type == AppType::kWeb) {
    web_app::UninstallImpl(web_app::WebAppProvider::GetForWebApps(profile_),
                           app_id, uninstall_source, parent_window);
  }
}

void AppServiceProxyLacros::UninstallSilently(
    const std::string& app_id,
    apps::mojom::UninstallSource uninstall_source) {
  NOTIMPLEMENTED();
}

void AppServiceProxyLacros::StopApp(const std::string& app_id) {
  NOTIMPLEMENTED();
}

void AppServiceProxyLacros::GetMenuModel(
    const std::string& app_id,
    apps::mojom::MenuType menu_type,
    int64_t display_id,
    apps::mojom::Publisher::GetMenuModelCallback callback) {
  NOTIMPLEMENTED();
}

void AppServiceProxyLacros::ExecuteContextMenuCommand(
    const std::string& app_id,
    int command_id,
    const std::string& shortcut_id,
    int64_t display_id) {
  NOTIMPLEMENTED();
}

void AppServiceProxyLacros::OpenNativeSettings(const std::string& app_id) {
  NOTIMPLEMENTED();
}

void AppServiceProxyLacros::FlushMojoCallsForTesting() {
  crosapi_receiver_.FlushForTesting();
}

apps::IconLoader* AppServiceProxyLacros::OverrideInnerIconLoaderForTesting(
    apps::IconLoader* icon_loader) {
  apps::IconLoader* old =
      inner_icon_loader_.overriding_icon_loader_for_testing_;
  inner_icon_loader_.overriding_icon_loader_for_testing_ = icon_loader;
  return old;
}

std::vector<std::string> AppServiceProxyLacros::GetAppIdsForUrl(
    const GURL& url,
    bool exclude_browsers,
    bool exclude_browser_tab_apps) {
  auto intent_launch_info =
      GetAppsForIntent(apps_util::CreateIntentFromUrl(url), exclude_browsers,
                       exclude_browser_tab_apps);
  std::vector<std::string> app_ids;
  for (auto& entry : intent_launch_info) {
    app_ids.push_back(std::move(entry.app_id));
  }
  return app_ids;
}

std::vector<IntentLaunchInfo> AppServiceProxyLacros::GetAppsForIntent(
    const apps::mojom::IntentPtr& mojom_intent,
    bool exclude_browsers,
    bool exclude_browser_tab_apps) {
  std::vector<IntentLaunchInfo> intent_launch_info;
  if (apps_util::OnlyShareToDrive(mojom_intent) ||
      !apps_util::IsIntentValid(mojom_intent)) {
    return intent_launch_info;
  }

  auto intent = ConvertMojomIntentToIntent(mojom_intent);
  if (!intent) {
    return intent_launch_info;
  }

  if (crosapi_receiver_.is_bound()) {
    app_registry_cache_.ForEachApp(
        [&intent_launch_info, &intent, &exclude_browsers,
         &exclude_browser_tab_apps](const apps::AppUpdate& update) {
          if (!apps_util::IsInstalled(update.Readiness()) ||
              !update.ShowInLauncher().value_or(false)) {
            return;
          }
          if (exclude_browser_tab_apps &&
              update.WindowMode() == WindowMode::kBrowser) {
            return;
          }
          std::set<std::string> existing_activities;
          for (const auto& filter : update.IntentFilters()) {
            DCHECK(filter);
            if (exclude_browsers && filter->IsBrowserFilter()) {
              continue;
            }
            if (intent->MatchFilter(filter)) {
              IntentLaunchInfo entry;
              entry.app_id = update.AppId();
              std::string activity_label;
              if (filter->activity_label &&
                  !filter->activity_label.value().empty()) {
                activity_label = filter->activity_label.value();
              } else {
                activity_label = update.Name();
              }
              if (base::Contains(existing_activities, activity_label)) {
                continue;
              }
              existing_activities.insert(activity_label);
              entry.activity_label = activity_label;
              entry.activity_name = filter->activity_name.value_or("");
              intent_launch_info.push_back(entry);
            }
          }
        });
  }
  return intent_launch_info;
}

std::vector<IntentLaunchInfo> AppServiceProxyLacros::GetAppsForFiles(
    const std::vector<GURL>& filesystem_urls,
    const std::vector<std::string>& mime_types) {
  return GetAppsForIntent(
      apps_util::CreateShareIntentFromFiles(filesystem_urls, mime_types));
}

void AppServiceProxyLacros::AddPreferredApp(const std::string& app_id,
                                            const GURL& url) {
  AddPreferredApp(app_id, apps_util::CreateIntentFromUrl(url));
}

void AppServiceProxyLacros::AddPreferredApp(
    const std::string& app_id,
    const apps::mojom::IntentPtr& intent) {
  if (!remote_crosapi_app_service_proxy_) {
    return;
  }

  // TODO(https://crbug.com/853604): Remove this and convert to a DCHECK
  // after finding out the root cause.
  if (app_id.empty()) {
    base::debug::DumpWithoutCrashing();
    return;
  }

  remote_crosapi_app_service_proxy_->AddPreferredApp(
      app_id, apps_util::ConvertAppServiceToCrosapiIntent(intent, profile_));
}

void AppServiceProxyLacros::SetSupportedLinksPreference(
    const std::string& app_id) {
  DCHECK(!app_id.empty());

  if (!remote_crosapi_app_service_proxy_) {
    return;
  }

  if (crosapi_app_service_proxy_version_ <
      int{crosapi::mojom::AppServiceProxy::MethodMinVersions::
              kSetSupportedLinksPreferenceMinVersion}) {
    LOG(WARNING) << "Ash AppServiceProxy version "
                 << crosapi_app_service_proxy_version_
                 << " does not support SetSupportedLinksPreference().";
    return;
  }

  remote_crosapi_app_service_proxy_->SetSupportedLinksPreference(app_id);
}

void AppServiceProxyLacros::RemoveSupportedLinksPreference(
    const std::string& app_id) {
  NOTIMPLEMENTED();
}

void AppServiceProxyLacros::SetWindowMode(const std::string& app_id,
                                          apps::mojom::WindowMode window_mode) {
  NOTIMPLEMENTED();
}

web_app::LacrosWebAppsController*
AppServiceProxyLacros::LacrosWebAppsControllerForTesting() {
  return lacros_web_apps_controller_.get();
}

void AppServiceProxyLacros::SetCrosapiAppServiceProxyForTesting(
    crosapi::mojom::AppServiceProxy* proxy) {
  remote_crosapi_app_service_proxy_ = proxy;
  // Set the proxy version to the newest version for testing.
  crosapi_app_service_proxy_version_ =
      crosapi::mojom::AppServiceProxy::Version_;
}

AppServiceProxyLacros::InnerIconLoader::InnerIconLoader(
    AppServiceProxyLacros* host)
    : host_(host), overriding_icon_loader_for_testing_(nullptr) {}

absl::optional<IconKey> AppServiceProxyLacros::InnerIconLoader::GetIconKey(
    const std::string& app_id) {
  if (overriding_icon_loader_for_testing_) {
    return overriding_icon_loader_for_testing_->GetIconKey(app_id);
  }

  if (!host_->crosapi_receiver_.is_bound()) {
    return absl::nullopt;
  }

  absl::optional<IconKey> icon_key;
  host_->app_registry_cache_.ForApp(
      app_id,
      [&icon_key](const AppUpdate& update) { icon_key = update.IconKey(); });
  return icon_key;
}

std::unique_ptr<IconLoader::Releaser>
AppServiceProxyLacros::InnerIconLoader::LoadIconFromIconKey(
    AppType app_type,
    const std::string& app_id,
    const IconKey& icon_key,
    IconType icon_type,
    int32_t size_hint_in_dip,
    bool allow_placeholder_icon,
    apps::LoadIconCallback callback) {
  if (overriding_icon_loader_for_testing_) {
    return overriding_icon_loader_for_testing_->LoadIconFromIconKey(
        app_type, app_id, icon_key, icon_type, size_hint_in_dip,
        allow_placeholder_icon, std::move(callback));
  }

  if (!host_->remote_crosapi_app_service_proxy_) {
    std::move(callback).Run(std::make_unique<IconValue>());
  } else if (host_->crosapi_app_service_proxy_version_ <
             int{crosapi::mojom::AppServiceProxy::MethodMinVersions::
                     kLoadIconMinVersion}) {
    LOG(WARNING) << "Ash AppServiceProxy version "
                 << host_->crosapi_app_service_proxy_version_
                 << " does not support LoadIcon().";
    std::move(callback).Run(std::make_unique<IconValue>());
  } else {
    host_->remote_crosapi_app_service_proxy_->LoadIcon(
        app_id, icon_key.Clone(), icon_type, size_hint_in_dip,
        std::move(callback));
  }
  return nullptr;
}

std::unique_ptr<IconLoader::Releaser>
AppServiceProxyLacros::InnerIconLoader::LoadIconFromIconKey(
    apps::mojom::AppType app_type,
    const std::string& app_id,
    apps::mojom::IconKeyPtr icon_key,
    apps::mojom::IconType icon_type,
    int32_t size_hint_in_dip,
    bool allow_placeholder_icon,
    apps::mojom::Publisher::LoadIconCallback callback) {
  if (overriding_icon_loader_for_testing_) {
    return overriding_icon_loader_for_testing_->LoadIconFromIconKey(
        app_type, app_id, std::move(icon_key), icon_type, size_hint_in_dip,
        allow_placeholder_icon, std::move(callback));
  }

  if (!host_->remote_crosapi_app_service_proxy_ || !icon_key) {
    std::move(callback).Run(apps::mojom::IconValue::New());
  } else if (host_->crosapi_app_service_proxy_version_ <
             int{crosapi::mojom::AppServiceProxy::MethodMinVersions::
                     kLoadIconMinVersion}) {
    LOG(WARNING) << "Ash AppServiceProxy version "
                 << host_->crosapi_app_service_proxy_version_
                 << " does not support LoadIcon().";
    std::move(callback).Run(apps::mojom::IconValue::New());
  } else {
    host_->remote_crosapi_app_service_proxy_->LoadIcon(
        app_id,
        std::make_unique<IconKey>(icon_key->timeline, icon_key->resource_id,
                                  icon_key->icon_effects),
        ConvertMojomIconTypeToIconType(icon_type), size_hint_in_dip,
        IconValueToMojomIconValueCallback(std::move(callback)));
  }
  return nullptr;
}

bool AppServiceProxyLacros::IsValidProfile() {
  if (!profile_) {
    return false;
  }

  // We only initialize the App Service for regular or guest profiles. Non-guest
  // off-the-record profiles do not get an instance.
  if (profile_->IsOffTheRecord() && !profile_->IsGuestSession()) {
    return false;
  }

  return true;
}

void AppServiceProxyLacros::Initialize() {
  if (remote_crosapi_app_service_proxy_) {
    return;
  }

  if (!IsValidProfile()) {
    return;
  }

  browser_app_launcher_ = std::make_unique<apps::BrowserAppLauncher>(profile_);

  if (profile_->IsMainProfile()) {
    lacros_web_apps_controller_ =
        std::make_unique<web_app::LacrosWebAppsController>(profile_);
    lacros_web_apps_controller_->Init();
  }

  // Make the chrome://app-icon/ resource available.
  content::URLDataSource::Add(profile_,
                              std::make_unique<apps::AppIconSource>(profile_));

  auto* service = chromeos::LacrosService::Get();

  if (!service || !service->IsAvailable<crosapi::mojom::AppServiceProxy>()) {
    return;
  }

  crosapi_app_service_proxy_version_ =
      service->GetInterfaceVersion(crosapi::mojom::AppServiceProxy::Uuid_);

  if (crosapi_app_service_proxy_version_ <
      int{crosapi::mojom::AppServiceProxy::MethodMinVersions::
              kRegisterAppServiceSubscriberMinVersion}) {
    LOG(WARNING) << "Ash AppServiceProxy version "
                 << crosapi_app_service_proxy_version_
                 << " does not support RegisterAppServiceSubscriber().";
    return;
  }

  service->GetRemote<crosapi::mojom::AppServiceProxy>()
      ->RegisterAppServiceSubscriber(
          crosapi_receiver_.BindNewPipeAndPassRemote());
  remote_crosapi_app_service_proxy_ =
      service->GetRemote<crosapi::mojom::AppServiceProxy>().get();
}

void AppServiceProxyLacros::Shutdown() {
  if (lacros_web_apps_controller_) {
    lacros_web_apps_controller_->Shutdown();
  }
}

void AppServiceProxyLacros::OnApps(std::vector<AppPtr> deltas,
                                   AppType app_type,
                                   bool should_notify_initialized) {
  std::vector<apps::mojom::AppPtr> mojom_deltas;
  for (const auto& app : deltas) {
    if (app) {
      mojom_deltas.push_back(ConvertAppToMojomApp(app));
    }
  }
  app_registry_cache_.OnApps(std::move(mojom_deltas),
                             ConvertAppTypeToMojomAppType(app_type),
                             should_notify_initialized);
  app_registry_cache_.OnApps(std::move(deltas), app_type,
                             should_notify_initialized);
}

void AppServiceProxyLacros::OnPreferredAppsChanged(
    apps::mojom::PreferredAppChangesPtr changes) {
  preferred_apps_list_.ApplyBulkUpdate(std::move(changes));
}

void AppServiceProxyLacros::InitializePreferredApps(
    PreferredAppsList::PreferredApps preferred_apps) {
  preferred_apps_list_.Init(preferred_apps);
}

}  // namespace apps
