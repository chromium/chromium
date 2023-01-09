// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_service_proxy_lacros.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_source.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/browser_app_instance_forwarder.h"
#include "chrome/browser/apps/app_service/browser_app_instance_tracker.h"
#include "chrome/browser/apps/app_service/intent_util.h"
#include "chrome/browser/apps/app_service/launch_result_type.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/apps/app_service/metrics/website_metrics_service_lacros.h"
#include "chrome/browser/apps/app_service/publishers/extension_apps.h"
#include "chrome/browser/extensions/extension_keeplist_chromeos.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/web_applications/app_service/lacros_web_apps_controller.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/services/app_service/public/cpp/preferred_app.h"
#include "components/services/app_service/public/cpp/types_util.h"
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

void AppServiceProxyLacros::ReinitializeForTesting(Profile* profile) {
  // Some test code creates a profile and profile-linked services, like the App
  // Service, before the profile is fully initialized. Such tests can call this
  // after full profile initialization to ensure the App Service implementation
  // has all of profile state it needs.
  crosapi_receiver_.reset();
  remote_crosapi_app_service_proxy_ = nullptr;
  profile_ = profile;
  is_using_testing_profile_ = true;
  app_registry_cache_.ReinitializeForTesting();  // IN-TEST

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

void AppServiceProxyLacros::Launch(const std::string& app_id,
                                   int32_t event_flags,
                                   apps::LaunchSource launch_source,
                                   apps::WindowInfoPtr window_info) {
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

  ProxyLaunch(CreateCrosapiLaunchParamsWithEventFlags(
      this, app_id, event_flags, launch_source, display::kInvalidDisplayId));
}

void AppServiceProxyLacros::LaunchAppWithFiles(
    const std::string& app_id,
    int32_t event_flags,
    LaunchSource launch_source,
    std::vector<base::FilePath> file_paths) {
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
  params->intent =
      apps_util::CreateCrosapiIntentForViewFiles(std::move(file_paths));
  ProxyLaunch(std::move(params));
}

void AppServiceProxyLacros::LaunchAppWithIntent(
    const std::string& app_id,
    int32_t event_flags,
    IntentPtr intent,
    LaunchSource launch_source,
    WindowInfoPtr window_info,
    base::OnceCallback<void(bool)> callback) {
  CHECK(intent);

  if (!remote_crosapi_app_service_proxy_) {
    std::move(callback).Run(false);
    return;
  }

  if (crosapi_app_service_proxy_version_ <
      int{crosapi::mojom::AppServiceProxy::MethodMinVersions::
              kLaunchMinVersion}) {
    LOG(WARNING) << "Ash AppServiceProxy version "
                 << crosapi_app_service_proxy_version_
                 << " does not support Launch().";
    std::move(callback).Run(false);
    return;
  }

  auto params = CreateCrosapiLaunchParamsWithEventFlags(
      this, app_id, event_flags, launch_source,
      window_info ? window_info->display_id : display::kInvalidDisplayId);
  params->intent =
      apps_util::ConvertAppServiceToCrosapiIntent(intent, profile_);
  ProxyLaunch(
      std::move(params),
      base::BindOnce(
          [](base::OnceCallback<void(bool)> callback, LaunchResult&& result) {
            std::move(callback).Run(ConvertLaunchResultToBool(result));
          },
          std::move(callback)));
}

void AppServiceProxyLacros::LaunchAppWithUrl(const std::string& app_id,
                                             int32_t event_flags,
                                             GURL url,
                                             LaunchSource launch_source,
                                             WindowInfoPtr window_info,
                                             LaunchCallback callback) {
  LaunchAppWithIntent(
      app_id, event_flags,
      std::make_unique<apps::Intent>(apps_util::kIntentActionView, url),
      launch_source, std::move(window_info),
      base::BindOnce(ConvertBoolToLaunchResult).Then(std::move(callback)));
}

void AppServiceProxyLacros::LaunchAppWithParams(AppLaunchParams&& params,
                                                LaunchCallback callback) {
  if (!remote_crosapi_app_service_proxy_) {
    std::move(callback).Run(LaunchResult());
    return;
  }

  if (crosapi_app_service_proxy_version_ <
      int{crosapi::mojom::AppServiceProxy::MethodMinVersions::
              kLaunchMinVersion}) {
    LOG(WARNING) << "Ash AppServiceProxy version "
                 << crosapi_app_service_proxy_version_
                 << " does not support Launch().";
    std::move(callback).Run(LaunchResult());
    return;
  }

  ProxyLaunch(ConvertLaunchParamsToCrosapi(params, profile_),
              std::move(callback));
}

void AppServiceProxyLacros::SetPermission(const std::string& app_id,
                                          PermissionPtr permission) {
  NOTIMPLEMENTED();
}

void AppServiceProxyLacros::Uninstall(const std::string& app_id,
                                      UninstallSource uninstall_source,
                                      gfx::NativeWindow parent_window) {
  // On non-ChromeOS, publishers run the remove dialog.
  auto app_type = app_registry_cache_.GetAppType(app_id);
  if (app_type == AppType::kWeb) {
    web_app::UninstallImpl(web_app::WebAppProvider::GetForWebApps(profile_),
                           app_id, uninstall_source, parent_window);
  } else {
    NOTIMPLEMENTED();
  }
}

void AppServiceProxyLacros::UninstallSilently(
    const std::string& app_id,
    UninstallSource uninstall_source) {
  if (!remote_crosapi_app_service_proxy_) {
    return;
  }

  if (crosapi_app_service_proxy_version_ <
      int{crosapi::mojom::AppServiceProxy::MethodMinVersions::
              kUninstallSilentlyMinVersion}) {
    LOG(WARNING) << "Ash AppServiceProxy version "
                 << crosapi_app_service_proxy_version_
                 << " does not support UninstallSilently().";
    return;
  }

  remote_crosapi_app_service_proxy_->UninstallSilently(app_id,
                                                       uninstall_source);
}

void AppServiceProxyLacros::StopApp(const std::string& app_id) {
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
  auto intent_launch_info = GetAppsForIntent(
      std::make_unique<apps::Intent>(apps_util::kIntentActionView, url),
      exclude_browsers, exclude_browser_tab_apps);
  std::vector<std::string> app_ids;
  for (auto& entry : intent_launch_info) {
    app_ids.push_back(std::move(entry.app_id));
  }
  return app_ids;
}

std::vector<IntentLaunchInfo> AppServiceProxyLacros::GetAppsForIntent(
    const IntentPtr& intent,
    bool exclude_browsers,
    bool exclude_browser_tab_apps) {
  std::vector<IntentLaunchInfo> intent_launch_info;
  if (!intent || intent->OnlyShareToDrive() || !intent->IsIntentValid()) {
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
      apps_util::MakeShareIntent(filesystem_urls, mime_types));
}

void AppServiceProxyLacros::AddPreferredApp(const std::string& app_id,
                                            const GURL& url) {
  AddPreferredApp(app_id, std::make_unique<apps::Intent>(
                              apps_util::kIntentActionView, url));
}

void AppServiceProxyLacros::AddPreferredApp(const std::string& app_id,
                                            const IntentPtr& intent) {
  if (!remote_crosapi_app_service_proxy_) {
    return;
  }

  DCHECK(!app_id.empty());

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
                                          WindowMode window_mode) {
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

void AppServiceProxyLacros::SetWebsiteMetricsServiceForTesting(
    std::unique_ptr<apps::WebsiteMetricsServiceLacros>
        website_metrics_service) {
  metrics_service_ = std::move(website_metrics_service);
}

base::WeakPtr<AppServiceProxyLacros> AppServiceProxyLacros::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

AppServiceProxyLacros::InnerIconLoader::InnerIconLoader(
    AppServiceProxyLacros* host)
    : host_(host) {}

absl::optional<IconKey> AppServiceProxyLacros::InnerIconLoader::GetIconKey(
    const std::string& app_id) {
  if (overriding_icon_loader_for_testing_) {
    return overriding_icon_loader_for_testing_->GetIconKey(app_id);
  }

  if (!host_->crosapi_receiver_.is_bound()) {
    return absl::nullopt;
  }

  absl::optional<IconKey> icon_key;
  host_->app_registry_cache_.ForOneApp(
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

  if (!profile_->AsTestingProfile()) {
    metrics_service_ = std::make_unique<WebsiteMetricsServiceLacros>(profile_);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&AppServiceProxyLacros::InitWebsiteMetrics,
                                  weak_ptr_factory_.GetWeakPtr()));
  }

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
  metrics_service_.reset();

  if (lacros_web_apps_controller_) {
    lacros_web_apps_controller_->Shutdown();
  }
}

void AppServiceProxyLacros::OnApps(std::vector<AppPtr> deltas,
                                   AppType app_type,
                                   bool should_notify_initialized) {
  app_registry_cache_.OnApps(std::move(deltas), app_type,
                             should_notify_initialized);
}

void AppServiceProxyLacros::OnPreferredAppsChanged(
    PreferredAppChangesPtr changes) {
  preferred_apps_list_.ApplyBulkUpdate(std::move(changes));
}

void AppServiceProxyLacros::InitializePreferredApps(
    PreferredApps preferred_apps) {
  preferred_apps_list_.Init(std::move(preferred_apps));
}

void AppServiceProxyLacros::ProxyLaunch(crosapi::mojom::LaunchParamsPtr params,
                                        LaunchCallback callback) {
  // Extensions that run in both the OS and standalone browser are not published
  // to the app service. Thus launching must happen directly.
  if (extensions::ExtensionRunsInBothOSAndStandaloneBrowser(params->app_id) ||
      extensions::ExtensionAppRunsInBothOSAndStandaloneBrowser(
          params->app_id)) {
    OpenApplication(profile_,
                    ConvertCrosapiToLaunchParams(std::move(params), profile_));
    std::move(callback).Run(ConvertBoolToLaunchResult(true));
    return;
  }
  if (crosapi_app_service_proxy_version_ <
      int{crosapi::mojom::AppServiceProxy::MethodMinVersions::
              kLaunchWithResultMinVersion}) {
    remote_crosapi_app_service_proxy_->Launch(std::move(params));
    std::move(callback).Run(ConvertBoolToLaunchResult(true));
  } else {
    remote_crosapi_app_service_proxy_->LaunchWithResult(
        std::move(params),
        LaunchResultToMojomLaunchResultCallback(std::move(callback)));
  }
}

void AppServiceProxyLacros::InitWebsiteMetrics() {
  if (metrics_service_) {
    metrics_service_->Start();
  }
}

}  // namespace apps
