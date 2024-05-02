// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_service_proxy_base.h"

#include <stddef.h>

#include <map>
#include <type_traits>
#include <utility>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "chrome/browser/apps/app_service/app_icon_source.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/apps/app_service/metrics/app_service_metrics.h"
#include "chrome/browser/apps/app_service/publishers/app_publisher.h"
#include "chrome/browser/profiles/profile.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/services/app_service/public/cpp/features.h"
#include "components/services/app_service/public/cpp/icon_effects.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "components/services/app_service/public/cpp/intent_filter.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/services/app_service/public/cpp/preferred_app.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "content/public/browser/url_data_source.h"
#include "url/gurl.h"

namespace apps {

class PreferredAppsListHandle;

namespace {

// Utility struct used in GetAppsForIntent.
struct IndexAndGeneric {
  size_t index;
  bool is_generic;
};

std::string GetActivityLabel(const IntentFilterPtr& filter,
                             const AppUpdate& update) {
  if (filter->activity_label.has_value() && !filter->activity_label->empty()) {
    return filter->activity_label.value();
  } else {
    return update.Name();
  }
}

}  // anonymous namespace

AppServiceProxyBase::AppInnerIconLoader::AppInnerIconLoader(
    AppServiceProxyBase* host)
    : host_(host), overriding_icon_loader_for_testing_(nullptr) {}

std::optional<IconKey> AppServiceProxyBase::AppInnerIconLoader::GetIconKey(
    const std::string& id) {
  if (overriding_icon_loader_for_testing_) {
    return overriding_icon_loader_for_testing_->GetIconKey(id);
  }

  std::optional<IconKey> icon_key;
  host_->app_registry_cache_.ForOneApp(
      id,
      [&icon_key](const AppUpdate& update) { icon_key = update.IconKey(); });
  return icon_key;
}

std::unique_ptr<IconLoader::Releaser>
AppServiceProxyBase::AppInnerIconLoader::LoadIconFromIconKey(
    const std::string& id,
    const IconKey& icon_key,
    IconType icon_type,
    int32_t size_hint_in_dip,
    bool allow_placeholder_icon,
    apps::LoadIconCallback callback) {
  if (overriding_icon_loader_for_testing_) {
    return overriding_icon_loader_for_testing_->LoadIconFromIconKey(
        id, icon_key, icon_type, size_hint_in_dip, allow_placeholder_icon,
        std::move(callback));
  }

  AppType app_type = host_->AppRegistryCache().GetAppType(id);
  if (host_->ShouldReadIcons(app_type)) {
    host_->ReadIcons(app_type, id, size_hint_in_dip, icon_key.Clone(),
                     icon_type, std::move(callback));
    return nullptr;
  }

  auto* publisher = host_->GetPublisher(app_type);
  if (!publisher) {
    LOG(WARNING) << "No publisher for requested icon";
    std::move(callback).Run(std::make_unique<IconValue>());
    return nullptr;
  }

  RecordIconLoadMethodMetrics(IconLoadingMethod::kViaNonMojomCall);
  publisher->LoadIcon(id, icon_key, icon_type, size_hint_in_dip,
                      allow_placeholder_icon, std::move(callback));
  return nullptr;
}

AppServiceProxyBase::AppServiceProxyBase(Profile* profile)
    : app_inner_icon_loader_(this),
      app_icon_coalescer_(&app_inner_icon_loader_),
      app_outer_icon_loader_(&app_icon_coalescer_,
                             IconCache::GarbageCollectionPolicy::kEager),
      profile_(profile) {
  preferred_apps_impl_ = std::make_unique<apps::PreferredAppsImpl>(
      this, profile ? profile->GetPath() : base::FilePath());
}

AppServiceProxyBase::~AppServiceProxyBase() = default;

void AppServiceProxyBase::ReinitializeForTesting(
    Profile* profile,
    base::OnceClosure read_completed_for_testing,
    base::OnceClosure write_completed_for_testing) {
  // Some test code creates a profile and profile-linked services, like the App
  // Service, before the profile is fully initialized. Such tests can call this
  // after full profile initialization to ensure the App Service implementation
  // has all of profile state it needs.
  profile_ = profile;
  is_using_testing_profile_ = true;
  app_registry_cache_.ReinitializeForTesting();  // IN-TEST

  preferred_apps_impl_ = std::make_unique<apps::PreferredAppsImpl>(
      this, profile ? profile->GetPath() : base::FilePath(),
      std::move(read_completed_for_testing),
      std::move(write_completed_for_testing));

  publishers_.clear();
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

  // Make the chrome://app-icon/ resource available.
  content::URLDataSource::Add(profile_,
                              std::make_unique<apps::AppIconSource>(profile_));
}

AppPublisher* AppServiceProxyBase::GetPublisher(AppType app_type) {
  auto it = publishers_.find(app_type);
  return it == publishers_.end() ? nullptr : it->second;
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

apps::PreferredAppsListHandle& AppServiceProxyBase::PreferredAppsList() {
  return preferred_apps_impl_->preferred_apps_list();
}

void AppServiceProxyBase::RegisterPublisher(AppType app_type,
                                            AppPublisher* publisher) {
  publishers_[app_type] = publisher;
}

void AppServiceProxyBase::UnregisterPublisher(AppType app_type) {
  publishers_.erase(app_type);
}

void AppServiceProxyBase::OnSupportedLinksPreferenceChanged(
    const std::string& app_id,
    bool open_in_app) {
  AppType app_type = AppRegistryCache().GetAppType(app_id);
  if (!base::Contains(publishers_, app_type)) {
    return;
  }

  publishers_[app_type]->OnSupportedLinksPreferenceChanged(app_id, open_in_app);
}

std::unique_ptr<IconLoader::Releaser> AppServiceProxyBase::LoadIcon(
    const std::string& app_id,
    const IconType& icon_type,
    int32_t size_hint_in_dip,
    bool allow_placeholder_icon,
    apps::LoadIconCallback callback) {
  return app_icon_loader()->LoadIcon(app_id, icon_type, size_hint_in_dip,
                                     allow_placeholder_icon,
                                     std::move(callback));
}

uint32_t AppServiceProxyBase::GetIconEffects(const std::string& app_id) {
  std::optional<apps::IconKey> icon_key = app_icon_loader()->GetIconKey(app_id);
  if (!icon_key.has_value()) {
    return IconEffects::kNone;
  }
  return icon_key->icon_effects;
}

std::unique_ptr<apps::IconLoader::Releaser>
AppServiceProxyBase::LoadIconWithIconEffects(const std::string& app_id,
                                             uint32_t icon_effects,
                                             IconType icon_type,
                                             int32_t size_hint_in_dip,
                                             bool allow_placeholder_icon,
                                             LoadIconCallback callback) {
  std::optional<apps::IconKey> icon_key = app_icon_loader()->GetIconKey(app_id);
  if (!icon_key.has_value()) {
    std::move(callback).Run(std::make_unique<IconValue>());
    return nullptr;
  }

  icon_key->icon_effects = icon_effects;

  return app_icon_loader()->LoadIconFromIconKey(
      app_id, icon_key.value(), icon_type, size_hint_in_dip,
      allow_placeholder_icon, std::move(callback));
}

void AppServiceProxyBase::Launch(const std::string& app_id,
                                 int32_t event_flags,
                                 apps::LaunchSource launch_source,
                                 apps::WindowInfoPtr window_info) {
  app_registry_cache_.ForOneApp(
      app_id, [this, event_flags, launch_source,
               &window_info](const apps::AppUpdate& update) {
        auto* publisher = GetPublisher(update.AppType());
        if (!publisher) {
          return;
        }

        if (MaybeShowLaunchPreventionDialog(update)) {
          return;
        }

        RecordAppLaunch(update.AppId(), launch_source);
        RecordAppPlatformMetrics(profile_, update, launch_source,
                                 apps::LaunchContainer::kLaunchContainerNone);

        publisher->Launch(update.AppId(), event_flags, launch_source,
                          std::move(window_info));

        PerformPostLaunchTasks(launch_source);
      });
}

void AppServiceProxyBase::LaunchAppWithFiles(
    const std::string& app_id,
    int32_t event_flags,
    LaunchSource launch_source,
    std::vector<base::FilePath> file_paths) {
  app_registry_cache_.ForOneApp(
      app_id, [this, event_flags, launch_source,
               &file_paths](const apps::AppUpdate& update) {
        auto* publisher = GetPublisher(update.AppType());
        if (!publisher) {
          return;
        }

        if (MaybeShowLaunchPreventionDialog(update)) {
          return;
        }

        RecordAppPlatformMetrics(profile_, update, launch_source,
                                 LaunchContainer::kLaunchContainerNone);

        // TODO(crbug/1117655): File manager records metrics for apps it
        // launched. So we only record launches from other places. We should
        // eventually move those metrics here, after AppService supports all
        // app types launched by file manager.
        if (launch_source != LaunchSource::kFromFileManager) {
          RecordAppLaunch(update.AppId(), launch_source);
        }

        publisher->LaunchAppWithFiles(update.AppId(), event_flags,
                                      launch_source, std::move(file_paths));

        PerformPostLaunchTasks(launch_source);
      });
}

void AppServiceProxyBase::LaunchAppWithIntent(const std::string& app_id,
                                              int32_t event_flags,
                                              IntentPtr intent,
                                              LaunchSource launch_source,
                                              WindowInfoPtr window_info,
                                              LaunchCallback callback) {
  CHECK(intent);
  app_registry_cache_.ForOneApp(app_id, [this, event_flags, &intent,
                                         launch_source, &window_info,
                                         callback = std::move(callback)](
                                            const AppUpdate& update) mutable {
    auto* publisher = GetPublisher(update.AppType());
    if (!publisher) {
      std::move(callback).Run(LaunchResult(State::kFailed));
      return;
    }

    if (MaybeShowLaunchPreventionDialog(update)) {
      std::move(callback).Run(LaunchResult(State::kFailed));
      return;
    }

    // TODO(crbug/1117655): File manager records metrics for apps it
    // launched. So we only record launches from other places. We should
    // eventually move those metrics here, after AppService supports all
    // app types launched by file manager.
    if (launch_source != LaunchSource::kFromFileManager) {
      RecordAppLaunch(update.AppId(), launch_source);
    }
    RecordAppPlatformMetrics(profile_, update, launch_source,
                             LaunchContainer::kLaunchContainerNone);

    publisher->LaunchAppWithIntent(update.AppId(), event_flags,
                                   std::move(intent), launch_source,
                                   std::move(window_info), std::move(callback));

    PerformPostLaunchTasks(launch_source);
  });
}

void AppServiceProxyBase::LaunchAppWithUrl(const std::string& app_id,
                                           int32_t event_flags,
                                           GURL url,
                                           LaunchSource launch_source,
                                           WindowInfoPtr window_info,
                                           LaunchCallback callback) {
  LaunchAppWithIntent(
      app_id, event_flags,
      std::make_unique<apps::Intent>(apps_util::kIntentActionView, url),
      launch_source, std::move(window_info), std::move(callback));
}

void AppServiceProxyBase::LaunchAppWithParams(AppLaunchParams&& params,
                                              LaunchCallback callback) {
  auto app_type = app_registry_cache_.GetAppType(params.app_id);
  auto* publisher = GetPublisher(app_type);
  if (!publisher) {
    std::move(callback).Run(LaunchResult());
    return;
  }

  app_registry_cache_.ForOneApp(
      params.app_id,
      [this, &params, &callback, &publisher](const apps::AppUpdate& update) {
        if (MaybeShowLaunchPreventionDialog(update)) {
          std::move(callback).Run(LaunchResult());
          return;
        }
        auto launch_source = params.launch_source;
        // TODO(crbug/1117655): File manager records metrics for apps it
        // launched. So we only record launches from other places. We should
        // eventually move those metrics here, after AppService supports all
        // app types launched by file manager.
        if (launch_source != apps::LaunchSource::kFromFileManager) {
          RecordAppLaunch(update.AppId(), launch_source);
        }

        RecordAppPlatformMetrics(profile_, update, launch_source,
                                 params.container);

        publisher->LaunchAppWithParams(
            std::move(params),
            base::BindOnce(&AppServiceProxyBase::OnLaunched,
                           weak_factory_.GetWeakPtr(), std::move(callback)));

        PerformPostLaunchTasks(launch_source);
      });
}

void AppServiceProxyBase::SetPermission(const std::string& app_id,
                                        PermissionPtr permission) {
  app_registry_cache_.ForOneApp(
      app_id, [this, &permission](const apps::AppUpdate& update) {
        auto* publisher = GetPublisher(update.AppType());
        if (!publisher) {
          return;
        }

        publisher->SetPermission(update.AppId(), std::move(permission));
      });
}

void AppServiceProxyBase::UninstallSilently(const std::string& app_id,
                                            UninstallSource uninstall_source) {
  auto app_type = app_registry_cache_.GetAppType(app_id);
  auto* publisher = GetPublisher(app_type);
  if (!publisher) {
    return;
  }
  publisher->Uninstall(app_id, uninstall_source,
                       /*clear_site_data=*/false, /*report_abuse=*/false);
  PerformPostUninstallTasks(app_type, app_id, uninstall_source);
}

void AppServiceProxyBase::StopApp(const std::string& app_id) {
  auto* publisher = GetPublisher(app_registry_cache_.GetAppType(app_id));
  if (publisher) {
    publisher->StopApp(app_id);
  }
}

void AppServiceProxyBase::GetMenuModel(
    const std::string& app_id,
    MenuType menu_type,
    int64_t display_id,
    base::OnceCallback<void(MenuItems)> callback) {
  auto* publisher = GetPublisher(app_registry_cache_.GetAppType(app_id));
  if (publisher) {
    publisher->GetMenuModel(app_id, menu_type, display_id, std::move(callback));
  } else {
    std::move(callback).Run(MenuItems());
  }
}

void AppServiceProxyBase::UpdateAppSize(const std::string& app_id) {
  auto app_type = app_registry_cache_.GetAppType(app_id);
  auto* publisher = GetPublisher(app_type);
  if (publisher) {
    publisher->UpdateAppSize(app_id);
  }
  return;
}

void AppServiceProxyBase::ExecuteContextMenuCommand(
    const std::string& app_id,
    int command_id,
    const std::string& shortcut_id,
    int64_t display_id) {
  auto* publisher = GetPublisher(app_registry_cache_.GetAppType(app_id));
  if (publisher) {
    publisher->ExecuteContextMenuCommand(app_id, command_id, shortcut_id,
                                         display_id);
  }
  return;
}

void AppServiceProxyBase::OpenNativeSettings(const std::string& app_id) {
  auto* publisher = GetPublisher(app_registry_cache_.GetAppType(app_id));
  if (publisher) {
    publisher->OpenNativeSettings(app_id);
  }
}

apps::IconLoader* AppServiceProxyBase::OverrideInnerIconLoaderForTesting(
    apps::IconLoader* icon_loader) {
  apps::IconLoader* old =
      app_inner_icon_loader_.overriding_icon_loader_for_testing_;
  app_inner_icon_loader_.overriding_icon_loader_for_testing_ = icon_loader;
  return old;
}

std::vector<std::string> AppServiceProxyBase::GetAppIdsForUrl(
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

std::vector<IntentLaunchInfo> AppServiceProxyBase::GetAppsForIntent(
    const apps::IntentPtr& intent,
    bool exclude_browsers,
    bool exclude_browser_tab_apps) {
  std::vector<IntentLaunchInfo> intent_launch_info;
  if (!intent || intent->OnlyShareToDrive() || !intent->IsIntentValid()) {
    return intent_launch_info;
  }

  app_registry_cache_.ForEachApp([this, &intent_launch_info, &intent,
                                  &exclude_browsers, &exclude_browser_tab_apps](
                                     const apps::AppUpdate& update) {
    if (update.Readiness() != apps::Readiness::kReady &&
        update.Readiness() != apps::Readiness::kDisabledByPolicy) {
      // We consider apps disabled by policy to be ready as they cause URL
      // loads to be blocked.
      return;
    }
    if (!update.HandlesIntents().value_or(false)) {
      return;
    }
    if (ShouldExcludeBrowserTabApps(exclude_browser_tab_apps,
                                    update.WindowMode())) {
      return;
    }
    // |activity_label| -> {index, is_generic}
    std::map<std::string, IndexAndGeneric> best_handler_map;
    bool is_file_handling_intent = !intent->files.empty();
    const apps::IntentFilters& filters = update.IntentFilters();
    for (size_t i = 0; i < filters.size(); i++) {
      const IntentFilterPtr& filter = filters[i];
      DCHECK(filter);
      if (exclude_browsers && filter->IsBrowserFilter()) {
        continue;
      }
      if (intent->MatchFilter(filter)) {
        // Return the first non-generic match if it exists, otherwise the
        // first generic match.
        bool generic = false;
        if (is_file_handling_intent) {
          generic = apps_util::IsGenericFileHandler(intent, filter);
        }
        std::string activity_label = GetActivityLabel(filter, update);
        // Replace the best handler if it is generic and we have a non-generic
        // one.
        auto it = best_handler_map.find(activity_label);
        if (it == best_handler_map.end() ||
            (it->second.is_generic && !generic)) {
          best_handler_map[activity_label] = IndexAndGeneric{i, generic};
        }
      }
    }
    for (const auto& handler_entry : best_handler_map) {
      const IntentFilterPtr& filter = filters[handler_entry.second.index];
      IntentLaunchInfo entry = CreateIntentLaunchInfo(intent, filter, update);
      intent_launch_info.push_back(entry);
    }
  });
  return intent_launch_info;
}

bool AppServiceProxyBase::ShouldExcludeBrowserTabApps(
    bool exclude_browser_tab_apps,
    WindowMode window_mode) {
  return (exclude_browser_tab_apps && window_mode == WindowMode::kBrowser);
}

std::vector<IntentLaunchInfo> AppServiceProxyBase::GetAppsForFiles(
    std::vector<apps::IntentFilePtr> files) {
  return GetAppsForIntent(std::make_unique<apps::Intent>(
                              apps_util::kIntentActionView, std::move(files)),
                          false, false);
}

void AppServiceProxyBase::SetSupportedLinksPreference(
    const std::string& app_id) {
  IntentFilters filters;
  AppRegistryCache().ForOneApp(
      app_id, [&app_id, &filters](const AppUpdate& app) {
        for (auto& filter : app.IntentFilters()) {
          if (apps_util::IsSupportedLinkForApp(app_id, filter)) {
            filters.push_back(std::move(filter));
          }
        }
      });

  SetSupportedLinksPreference(app_id, std::move(filters));
}

void AppServiceProxyBase::SetSupportedLinksPreference(
    const std::string& app_id,
    IntentFilters all_link_filters) {
  DCHECK(!app_id.empty());

  preferred_apps_impl_->SetSupportedLinksPreference(
      app_id, std::move(all_link_filters));
}

void AppServiceProxyBase::RemoveSupportedLinksPreference(
    const std::string& app_id) {
  DCHECK(!app_id.empty());

  preferred_apps_impl_->RemoveSupportedLinksPreference(app_id);
}

void AppServiceProxyBase::SetWindowMode(const std::string& app_id,
                                        WindowMode window_mode) {
  auto* publisher = GetPublisher(app_registry_cache_.GetAppType(app_id));
  if (publisher) {
    publisher->SetWindowMode(app_id, window_mode);
  }
}

void AppServiceProxyBase::OnApps(std::vector<AppPtr> deltas,
                                 AppType app_type,
                                 bool should_notify_initialized) {
  for (const auto& delta : deltas) {
    if (delta->readiness != Readiness::kUnknown &&
        !apps_util::IsInstalled(delta->readiness)) {
      preferred_apps_impl_->RemovePreferredApp(delta->app_id);
    }
  }

  app_registry_cache_.OnApps(std::move(deltas), app_type,
                             should_notify_initialized);
}

void AppServiceProxyBase::OnCapabilityAccesses(
    std::vector<CapabilityAccessPtr> deltas) {
  app_capability_access_cache_.OnCapabilityAccesses(std::move(deltas));
}

IntentFilterPtr AppServiceProxyBase::FindBestMatchingFilter(
    const IntentPtr& intent) {
  IntentFilterPtr best_matching_intent_filter;
  if (!intent) {
    return best_matching_intent_filter;
  }

  int best_match_level = static_cast<int>(IntentFilterMatchLevel::kNone);
  app_registry_cache_.ForEachApp(
      [&intent, &best_match_level,
       &best_matching_intent_filter](const apps::AppUpdate& update) {
        for (auto& filter : update.IntentFilters()) {
          if (!intent->MatchFilter(filter)) {
            continue;
          }
          auto match_level = filter->GetFilterMatchLevel();
          if (match_level <= best_match_level) {
            continue;
          }
          best_matching_intent_filter = std::move(filter);
          best_match_level = match_level;
        }
      });
  return best_matching_intent_filter;
}

void AppServiceProxyBase::PerformPostLaunchTasks(
    apps::LaunchSource launch_source) {}

void AppServiceProxyBase::RecordAppPlatformMetrics(
    Profile* profile,
    const apps::AppUpdate& update,
    apps::LaunchSource launch_source,
    apps::LaunchContainer container) {}

void AppServiceProxyBase::PerformPostUninstallTasks(
    apps::AppType app_type,
    const std::string& app_id,
    UninstallSource uninstall_source) {}

void AppServiceProxyBase::OnLaunched(LaunchCallback callback,
                                     LaunchResult&& launch_result) {
  std::move(callback).Run(std::move(launch_result));
}

bool AppServiceProxyBase::ShouldReadIcons(AppType app_type) {
  return false;
}

IntentLaunchInfo AppServiceProxyBase::CreateIntentLaunchInfo(
    const apps::IntentPtr& intent,
    const apps::IntentFilterPtr& filter,
    const apps::AppUpdate& update) {
  IntentLaunchInfo entry;
  entry.app_id = update.AppId();
  entry.activity_label = GetActivityLabel(filter, update);
  entry.activity_name = filter->activity_name.value_or("");
  entry.is_generic_file_handler =
      apps_util::IsGenericFileHandler(intent, filter);
  entry.is_file_extension_match = filter->IsFileExtensionsFilter();
  return entry;
}

IntentLaunchInfo::IntentLaunchInfo() = default;
IntentLaunchInfo::~IntentLaunchInfo() = default;
IntentLaunchInfo::IntentLaunchInfo(const IntentLaunchInfo& other) = default;

}  // namespace apps
