// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_service_proxy_base.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/containers/contains.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_icon_source.h"
#include "chrome/browser/apps/app_service/app_service_metrics.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "components/services/app_service/app_service_impl.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "content/public/browser/url_data_source.h"
#include "ui/display/types/display_constants.h"
#include "url/url_constants.h"

namespace apps {

AppServiceProxyBase::InnerIconLoader::InnerIconLoader(AppServiceProxyBase* host)
    : host_(host), overriding_icon_loader_for_testing_(nullptr) {}

apps::mojom::IconKeyPtr AppServiceProxyBase::InnerIconLoader::GetIconKey(
    const std::string& app_id) {
  if (overriding_icon_loader_for_testing_) {
    return overriding_icon_loader_for_testing_->GetIconKey(app_id);
  }

  apps::mojom::IconKeyPtr icon_key;
  if (host_->app_service_.is_connected()) {
    host_->app_registry_cache_.ForOneApp(
        app_id, [&icon_key](const apps::AppUpdate& update) {
          icon_key = update.IconKey();
        });
  }
  return icon_key;
}

std::unique_ptr<IconLoader::Releaser>
AppServiceProxyBase::InnerIconLoader::LoadIconFromIconKey(
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

  if (host_->app_service_.is_connected() && icon_key) {
    // TODO(crbug.com/826982): Mojo doesn't guarantee the order of messages,
    // so multiple calls to this method might not resolve their callbacks in
    // order. As per khmel@, "you may have race here, assume you publish change
    // for the app and app requested new icon. But new icon is not delivered
    // yet and you resolve old one instead. Now new icon arrives asynchronously
    // but you no longer notify the app or do?"
    host_->app_service_->LoadIcon(app_type, app_id, std::move(icon_key),
                                  icon_type, size_hint_in_dip,
                                  allow_placeholder_icon, std::move(callback));
  } else {
    std::move(callback).Run(apps::mojom::IconValue::New());
  }
  return nullptr;
}

AppServiceProxyBase::AppServiceProxyBase(Profile* profile)
    : inner_icon_loader_(this),
      icon_coalescer_(&inner_icon_loader_),
      outer_icon_loader_(&icon_coalescer_,
                         apps::IconCache::GarbageCollectionPolicy::kEager),
      profile_(profile) {}

AppServiceProxyBase::~AppServiceProxyBase() = default;

void AppServiceProxyBase::ReInitializeForTesting(Profile* profile) {
  // Some test code creates a profile and profile-linked services, like the App
  // Service, before the profile is fully initialized. Such tests can call this
  // after full profile initialization to ensure the App Service implementation
  // has all of profile state it needs.
  app_service_.reset();
  profile_ = profile;
  is_using_testing_profile_ = true;
  Initialize();
}

bool AppServiceProxyBase::IsValidProfile() {
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

void AppServiceProxyBase::Initialize() {
  if (!IsValidProfile()) {
    return;
  }

  browser_app_launcher_ = std::make_unique<apps::BrowserAppLauncher>(profile_);

  app_service_impl_ = std::make_unique<apps::AppServiceImpl>(
      profile_->GetPath(),
      base::FeatureList::IsEnabled(features::kIntentHandlingSharing));
  app_service_impl_->BindReceiver(app_service_.BindNewPipeAndPassReceiver());

  if (app_service_.is_connected()) {
    // The AppServiceProxy is a subscriber: something that wants to be able to
    // list all known apps.
    mojo::PendingRemote<apps::mojom::Subscriber> subscriber;
    receivers_.Add(this, subscriber.InitWithNewPipeAndPassReceiver());
    app_service_->RegisterSubscriber(std::move(subscriber), nullptr);
  }

  Observe(&app_registry_cache_);
}

mojo::Remote<apps::mojom::AppService>& AppServiceProxyBase::AppService() {
  return app_service_;
}

apps::AppRegistryCache& AppServiceProxyBase::AppRegistryCache() {
  return app_registry_cache_;
}

apps::AppCapabilityAccessCache&
AppServiceProxyBase::AppCapabilityAccessCache() {
  return app_capability_access_cache_;
}

BrowserAppLauncher* AppServiceProxyBase::BrowserAppLauncher() {
  return browser_app_launcher_.get();
}

apps::PreferredAppsList& AppServiceProxyBase::PreferredApps() {
  return preferred_apps_;
}

apps::mojom::IconKeyPtr AppServiceProxyBase::GetIconKey(
    const std::string& app_id) {
  return outer_icon_loader_.GetIconKey(app_id);
}

std::unique_ptr<apps::IconLoader::Releaser>
AppServiceProxyBase::LoadIconFromIconKey(
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

void AppServiceProxyBase::Launch(const std::string& app_id,
                                 int32_t event_flags,
                                 apps::mojom::LaunchSource launch_source,
                                 apps::mojom::WindowInfoPtr window_info) {
  if (app_service_.is_connected()) {
    app_registry_cache_.ForOneApp(
        app_id, [this, event_flags, launch_source,
                 &window_info](const apps::AppUpdate& update) {
          if (MaybeShowLaunchPreventionDialog(update)) {
            return;
          }

          RecordAppLaunch(update.AppId(), launch_source);

          app_service_->Launch(update.AppType(), update.AppId(), event_flags,
                               launch_source, std::move(window_info));
        });
  }
}

void AppServiceProxyBase::LaunchAppWithFiles(
    const std::string& app_id,
    apps::mojom::LaunchContainer container,
    int32_t event_flags,
    apps::mojom::LaunchSource launch_source,
    apps::mojom::FilePathsPtr file_paths) {
  if (app_service_.is_connected()) {
    app_registry_cache_.ForOneApp(
        app_id, [this, container, event_flags, launch_source,
                 &file_paths](const apps::AppUpdate& update) {
          if (MaybeShowLaunchPreventionDialog(update)) {
            return;
          }

          // TODO(crbug/1117655): Presently, app launch metrics are recorded in
          // the caller. We should record them here, with the same SWA logic as
          // AppServiceProxy::Launch. There is an if statement to detect
          // launches from the file manager in LaunchSystemWebApp that should be
          // removed at the same time.
          app_service_->LaunchAppWithFiles(
              update.AppType(), update.AppId(), container, event_flags,
              launch_source, std::move(file_paths));
        });
  }
}

void AppServiceProxyBase::LaunchAppWithFileUrls(
    const std::string& app_id,
    int32_t event_flags,
    apps::mojom::LaunchSource launch_source,
    const std::vector<GURL>& file_urls,
    const std::vector<std::string>& mime_types) {
  LaunchAppWithIntent(
      app_id, event_flags,
      apps_util::CreateShareIntentFromFiles(file_urls, mime_types),
      launch_source, MakeWindowInfo(display::kDefaultDisplayId));
}

void AppServiceProxyBase::LaunchAppWithIntent(
    const std::string& app_id,
    int32_t event_flags,
    apps::mojom::IntentPtr intent,
    apps::mojom::LaunchSource launch_source,
    apps::mojom::WindowInfoPtr window_info) {
  if (app_service_.is_connected()) {
    app_registry_cache_.ForOneApp(
        app_id, [this, event_flags, &intent, launch_source,
                 &window_info](const apps::AppUpdate& update) {
          if (MaybeShowLaunchPreventionDialog(update)) {
            return;
          }

          RecordAppLaunch(update.AppId(), launch_source);

          app_service_->LaunchAppWithIntent(
              update.AppType(), update.AppId(), event_flags, std::move(intent),
              launch_source, std::move(window_info));
        });
  }
}

void AppServiceProxyBase::LaunchAppWithUrl(
    const std::string& app_id,
    int32_t event_flags,
    GURL url,
    apps::mojom::LaunchSource launch_source,
    apps::mojom::WindowInfoPtr window_info) {
  LaunchAppWithIntent(app_id, event_flags, apps_util::CreateIntentFromUrl(url),
                      launch_source, std::move(window_info));
}

void AppServiceProxyBase::SetPermission(const std::string& app_id,
                                        apps::mojom::PermissionPtr permission) {
  if (app_service_.is_connected()) {
    app_registry_cache_.ForOneApp(
        app_id, [this, &permission](const apps::AppUpdate& update) {
          app_service_->SetPermission(update.AppType(), update.AppId(),
                                      std::move(permission));
        });
  }
}

void AppServiceProxyBase::UninstallSilently(
    const std::string& app_id,
    apps::mojom::UninstallSource uninstall_source) {
  if (app_service_.is_connected()) {
    app_service_->Uninstall(app_registry_cache_.GetAppType(app_id), app_id,
                            uninstall_source,
                            /*clear_site_data=*/false, /*report_abuse=*/false);
  }
}

void AppServiceProxyBase::StopApp(const std::string& app_id) {
  if (!app_service_.is_connected()) {
    return;
  }
  apps::mojom::AppType app_type = app_registry_cache_.GetAppType(app_id);
  app_service_->StopApp(app_type, app_id);
}

void AppServiceProxyBase::GetMenuModel(
    const std::string& app_id,
    apps::mojom::MenuType menu_type,
    int64_t display_id,
    apps::mojom::Publisher::GetMenuModelCallback callback) {
  if (!app_service_.is_connected()) {
    return;
  }

  apps::mojom::AppType app_type = app_registry_cache_.GetAppType(app_id);
  app_service_->GetMenuModel(app_type, app_id, menu_type, display_id,
                             std::move(callback));
}

void AppServiceProxyBase::ExecuteContextMenuCommand(
    const std::string& app_id,
    int command_id,
    const std::string& shortcut_id,
    int64_t display_id) {
  if (!app_service_.is_connected()) {
    return;
  }

  apps::mojom::AppType app_type = app_registry_cache_.GetAppType(app_id);
  app_service_->ExecuteContextMenuCommand(app_type, app_id, command_id,
                                          shortcut_id, display_id);
}

void AppServiceProxyBase::OpenNativeSettings(const std::string& app_id) {
  if (app_service_.is_connected()) {
    app_registry_cache_.ForOneApp(
        app_id, [this](const apps::AppUpdate& update) {
          app_service_->OpenNativeSettings(update.AppType(), update.AppId());
        });
  }
}

apps::IconLoader* AppServiceProxyBase::OverrideInnerIconLoaderForTesting(
    apps::IconLoader* icon_loader) {
  apps::IconLoader* old =
      inner_icon_loader_.overriding_icon_loader_for_testing_;
  inner_icon_loader_.overriding_icon_loader_for_testing_ = icon_loader;
  return old;
}

std::vector<std::string> AppServiceProxyBase::GetAppIdsForUrl(
    const GURL& url,
    bool exclude_browsers) {
  auto intent_launch_info =
      GetAppsForIntent(apps_util::CreateIntentFromUrl(url), exclude_browsers);
  std::vector<std::string> app_ids;
  for (auto& entry : intent_launch_info) {
    app_ids.push_back(std::move(entry.app_id));
  }
  return app_ids;
}

std::vector<IntentLaunchInfo> AppServiceProxyBase::GetAppsForIntent(
    const apps::mojom::IntentPtr& intent,
    bool exclude_browsers) {
  std::vector<IntentLaunchInfo> intent_launch_info;
  if (apps_util::OnlyShareToDrive(intent) ||
      !apps_util::IsIntentValid(intent)) {
    return intent_launch_info;
  }

  if (app_service_.is_bound()) {
    app_registry_cache_.ForEachApp([&intent_launch_info, &intent,
                                    &exclude_browsers](
                                       const apps::AppUpdate& update) {
      if (update.Readiness() == apps::mojom::Readiness::kUninstalledByUser) {
        return;
      }
      std::set<std::string> existing_activities;
      for (const auto& filter : update.IntentFilters()) {
        if (exclude_browsers && apps_util::IsBrowserFilter(filter)) {
          continue;
        }
        if (apps_util::IntentMatchesFilter(intent, filter)) {
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

std::vector<IntentLaunchInfo> AppServiceProxyBase::GetAppsForFiles(
    const std::vector<GURL>& filesystem_urls,
    const std::vector<std::string>& mime_types) {
  return GetAppsForIntent(
      apps_util::CreateShareIntentFromFiles(filesystem_urls, mime_types));
}

void AppServiceProxyBase::AddPreferredApp(const std::string& app_id,
                                          const GURL& url) {
  AddPreferredApp(app_id, apps_util::CreateIntentFromUrl(url));
}

void AppServiceProxyBase::AddPreferredApp(
    const std::string& app_id,
    const apps::mojom::IntentPtr& intent) {
  // TODO(https://crbug.com/853604): Remove this and convert to a DCHECK
  // after finding out the root cause.
  if (app_id.empty()) {
    base::debug::DumpWithoutCrashing();
    return;
  }
  auto intent_filter = FindBestMatchingFilter(intent);
  if (!intent_filter) {
    return;
  }
  preferred_apps_.AddPreferredApp(app_id, intent_filter);
  if (app_service_.is_connected()) {
    constexpr bool kFromPublisher = false;
    app_service_->AddPreferredApp(app_registry_cache_.GetAppType(app_id),
                                  app_id, std::move(intent_filter),
                                  intent->Clone(), kFromPublisher);
  }
}

void AppServiceProxyBase::AddAppIconSource(Profile* profile) {
  // Make the chrome://app-icon/ resource available.
  content::URLDataSource::Add(profile,
                              std::make_unique<apps::AppIconSource>(profile));
}

void AppServiceProxyBase::OnApps(std::vector<apps::mojom::AppPtr> deltas,
                                 apps::mojom::AppType app_type,
                                 bool should_notify_initialized) {
  app_registry_cache_.OnApps(std::move(deltas), app_type,
                             should_notify_initialized);
}

void AppServiceProxyBase::OnCapabilityAccesses(
    std::vector<apps::mojom::CapabilityAccessPtr> deltas) {
  app_capability_access_cache_.OnCapabilityAccesses(std::move(deltas));
}

void AppServiceProxyBase::Clone(
    mojo::PendingReceiver<apps::mojom::Subscriber> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void AppServiceProxyBase::OnPreferredAppSet(
    const std::string& app_id,
    apps::mojom::IntentFilterPtr intent_filter) {
  preferred_apps_.AddPreferredApp(app_id, intent_filter);
}

void AppServiceProxyBase::OnPreferredAppRemoved(
    const std::string& app_id,
    apps::mojom::IntentFilterPtr intent_filter) {
  preferred_apps_.DeletePreferredApp(app_id, intent_filter);
}

void AppServiceProxyBase::InitializePreferredApps(
    PreferredAppsList::PreferredApps preferred_apps) {
  preferred_apps_.Init(preferred_apps);
}

void AppServiceProxyBase::OnAppUpdate(const apps::AppUpdate& update) {
  if (!update.ReadinessChanged() ||
      update.Readiness() != apps::mojom::Readiness::kUninstalledByUser) {
    return;
  }
  preferred_apps_.DeleteAppId(update.AppId());
}

void AppServiceProxyBase::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  Observe(nullptr);
}

apps::mojom::IntentFilterPtr AppServiceProxyBase::FindBestMatchingFilter(
    const apps::mojom::IntentPtr& intent) {
  apps::mojom::IntentFilterPtr best_matching_intent_filter;
  if (!app_service_.is_bound()) {
    return best_matching_intent_filter;
  }

  int best_match_level = apps_util::IntentFilterMatchLevel::kNone;
  app_registry_cache_.ForEachApp(
      [&intent, &best_match_level,
       &best_matching_intent_filter](const apps::AppUpdate& update) {
        for (const auto& filter : update.IntentFilters()) {
          if (!apps_util::IntentMatchesFilter(intent, filter)) {
            continue;
          }
          auto match_level = apps_util::GetFilterMatchLevel(filter);
          if (match_level <= best_match_level) {
            continue;
          }
          best_matching_intent_filter = filter->Clone();
          best_match_level = match_level;
        }
      });
  return best_matching_intent_filter;
}

}  // namespace apps
