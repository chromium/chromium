// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_service_proxy_ash.h"

#include <utility>

#include "ash/constants/ash_features.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_util.h"
#include "chrome/browser/apps/app_service/app_icon/icon_effects.h"
#include "chrome/browser/apps/app_service/browser_app_instance_registry.h"
#include "chrome/browser/apps/app_service/browser_app_instance_tracker.h"
#include "chrome/browser/apps/app_service/instance_registry_updater.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics_service.h"
#include "chrome/browser/apps/app_service/metrics/app_service_metrics.h"
#include "chrome/browser/apps/app_service/package_id.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_registry_cache.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_service.h"
#include "chrome/browser/apps/app_service/publishers/app_publisher.h"
#include "chrome/browser/apps/app_service/publishers/shortcut_publisher.h"
#include "chrome/browser/apps/app_service/publishers/standalone_browser_apps.h"
#include "chrome/browser/apps/app_service/shortcut_removal_dialog.h"
#include "chrome/browser/apps/app_service/uninstall_dialog.h"
#include "chrome/browser/ash/app_restore/full_restore_service.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_time_limit_interface.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/ash/policy/dlp/dlp_files_controller_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#include "components/account_id/account_id.h"
#include "components/app_constants/constants.h"
#include "components/app_restore/full_restore_save_handler.h"
#include "components/app_restore/full_restore_utils.h"
#include "components/grit/components_resources.h"
#include "components/services/app_service/public/cpp/app_capability_access_cache_wrapper.h"
#include "components/services/app_service/public/cpp/app_registry_cache_wrapper.h"
#include "components/services/app_service/public/cpp/features.h"
#include "components/services/app_service/public/cpp/preferred_apps_impl.h"
#include "components/services/app_service/public/cpp/preferred_apps_list.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut_registry_cache.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "extensions/grit/extensions_browser_resources.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {
constexpr int32_t kAppDialogIconSize = 48;
constexpr int32_t kAppDialogIconBadgeSize = 24;
}  // namespace

namespace apps {
AppServiceProxyAsh::AppServiceProxyAsh(Profile* profile)
    : AppServiceProxyBase(profile),
      shortcut_inner_icon_loader_(this),
      shortcut_icon_coalescer_(&shortcut_inner_icon_loader_),
      shortcut_outer_icon_loader_(&shortcut_icon_coalescer_,
                                  IconCache::GarbageCollectionPolicy::kEager),
      icon_reader_(profile),
      icon_writer_(profile) {
  if (web_app::IsWebAppsCrosapiEnabled()) {
    browser_app_instance_tracker_ =
        std::make_unique<apps::BrowserAppInstanceTracker>(profile_,
                                                          app_registry_cache_);
    browser_app_instance_registry_ =
        std::make_unique<apps::BrowserAppInstanceRegistry>(
            *browser_app_instance_tracker_);
    browser_app_instance_app_service_updater_ =
        std::make_unique<apps::InstanceRegistryUpdater>(
            *browser_app_instance_registry_, instance_registry_);
  }
  instance_registry_observer_.Observe(&instance_registry_);
}

AppServiceProxyAsh::~AppServiceProxyAsh() {
  if (IsValidProfile()) {
    ::full_restore::FullRestoreSaveHandler::GetInstance()->SetAppRegistryCache(
        profile_->GetPath(), nullptr);
  }

  AppCapabilityAccessCacheWrapper::Get().RemoveAppCapabilityAccessCache(
      &app_capability_access_cache_);
  AppRegistryCacheWrapper::Get().RemoveAppRegistryCache(&app_registry_cache_);
}

bool AppServiceProxyAsh::IsValidProfile() {
  if (!profile_) {
    return false;
  }

  // Use OTR profile for Guest Session.
  if (profile_->IsGuestSession()) {
    return profile_->IsOffTheRecord();
  }

  return AppServiceProxyBase::IsValidProfile();
}

void AppServiceProxyAsh::Initialize() {
  if (!IsValidProfile()) {
    return;
  }

  if (base::FeatureList::IsEnabled(kAppServiceStorage)) {
    app_storage_ = std::make_unique<apps::AppStorage>(profile_->GetPath(),
                                                      app_registry_cache_);
  }

  const user_manager::User* user =
      ash::ProfileHelper::Get()->GetUserByProfile(profile_);
  if (user) {
    const AccountId& account_id = user->GetAccountId();
    app_registry_cache_.SetAccountId(account_id);
    AppRegistryCacheWrapper::Get().AddAppRegistryCache(account_id,
                                                       &app_registry_cache_);
    app_capability_access_cache_.SetAccountId(account_id);
    AppCapabilityAccessCacheWrapper::Get().AddAppCapabilityAccessCache(
        account_id, &app_capability_access_cache_);
  }

  if (user == user_manager::UserManager::Get()->GetPrimaryUser()) {
    ::full_restore::SetPrimaryProfilePath(profile_->GetPath());

    // In Multi-Profile mode, only set for the primary user. For other users,
    // active profile path is set when switch users.
    ::full_restore::SetActiveProfilePath(profile_->GetPath());
  }

  ::full_restore::FullRestoreSaveHandler::GetInstance()->SetAppRegistryCache(
      profile_->GetPath(), &app_registry_cache_);

  AppServiceProxyBase::Initialize();

  auto* cache = &AppRegistryCache();
  if (!app_registry_cache_observer_.IsObservingSource(cache)) {
    app_registry_cache_observer_.Reset();
    app_registry_cache_observer_.Observe(cache);
  }

  publisher_host_ = std::make_unique<PublisherHost>(this);

  if (crosapi::browser_util::IsLacrosEnabled() &&
      ash::ProfileHelper::IsPrimaryProfile(profile_) &&
      web_app::IsWebAppsCrosapiEnabled()) {
    auto* browser_manager = crosapi::BrowserManager::Get();
    // In unit tests, it is possible that the browser manager is not created.
    if (browser_manager) {
      keep_alive_ = browser_manager->KeepAlive(
          crosapi::BrowserManager::Feature::kAppService);
    }
  }
  if (!profile_->AsTestingProfile() &&
      (!::ash::features::IsShimlessRMA3pDiagnosticsEnabled() ||
       !::ash::IsShimlessRmaAppBrowserContext(profile_))) {
    app_platform_metrics_service_ =
        std::make_unique<apps::AppPlatformMetricsService>(profile_);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&AppServiceProxyAsh::InitAppPlatformMetrics,
                                  weak_ptr_factory_.GetWeakPtr()));
  }
  if (ash::features::ArePromiseIconsEnabled()) {
    promise_app_service_ = std::make_unique<apps::PromiseAppService>(
        profile_, app_registry_cache_);
  }
  if (base::FeatureList::IsEnabled(features::kCrosWebAppShortcutUiUpdate)) {
    shortcut_registry_cache_ = std::make_unique<apps::ShortcutRegistryCache>();
  }
}

apps::InstanceRegistry& AppServiceProxyAsh::InstanceRegistry() {
  return instance_registry_;
}

apps::AppPlatformMetrics* AppServiceProxyAsh::AppPlatformMetrics() {
  return app_platform_metrics_service_
             ? app_platform_metrics_service_->AppPlatformMetrics()
             : nullptr;
}

apps::AppPlatformMetricsService*
AppServiceProxyAsh::AppPlatformMetricsService() {
  return app_platform_metrics_service_ ? app_platform_metrics_service_.get()
                                       : nullptr;
}

apps::BrowserAppInstanceTracker*
AppServiceProxyAsh::BrowserAppInstanceTracker() {
  return browser_app_instance_tracker_.get();
}

apps::BrowserAppInstanceRegistry*
AppServiceProxyAsh::BrowserAppInstanceRegistry() {
  return browser_app_instance_registry_.get();
}

apps::StandaloneBrowserApps* AppServiceProxyAsh::StandaloneBrowserApps() {
  return publisher_host_ ? publisher_host_->StandaloneBrowserApps() : nullptr;
}

void AppServiceProxyAsh::RegisterCrosApiSubScriber(
    SubscriberCrosapi* subscriber) {
  crosapi_subscriber_ = subscriber;

  crosapi_subscriber_->InitializeApps();

  // Initialise the Preferred Apps in the `crosapi_subscriber_` on register.
  if (preferred_apps_impl_ &&
      preferred_apps_impl_->preferred_apps_list().IsInitialized()) {
    crosapi_subscriber_->InitializePreferredApps(
        preferred_apps_impl_->preferred_apps_list().GetValue());
  }
}

void AppServiceProxyAsh::Uninstall(const std::string& app_id,
                                   UninstallSource uninstall_source,
                                   gfx::NativeWindow parent_window) {
  UninstallImpl(app_id, uninstall_source, parent_window, base::DoNothing());
}

void AppServiceProxyAsh::OnApps(std::vector<AppPtr> deltas,
                                AppType app_type,
                                bool should_notify_initialized) {
  // Delete app icon folders for uninstalled apps or the icon updated app.
  std::vector<std::string> app_ids;
  for (const auto& delta : deltas) {
    if ((delta->readiness != Readiness::kUnknown &&
         !apps_util::IsInstalled(delta->readiness)) ||
        (delta->icon_key.has_value() && delta->icon_key->raw_icon_updated)) {
      // If there's already a deletion in progress, skip the deletion request.
      if (base::Contains(pending_read_icon_requests_, delta->app_id)) {
        continue;
      }

      app_ids.push_back(delta->app_id);
      pending_read_icon_requests_[delta->app_id] =
          std::vector<base::OnceCallback<void()>>();
    }
  }

  if (!app_ids.empty()) {
    ScheduleIconFoldersDeletion(
        profile_->GetPath(), app_ids,
        base::BindOnce(&AppServiceProxyAsh::PostIconFoldersDeletion,
                       weak_ptr_factory_.GetWeakPtr(), app_ids));
  }

  // Close uninstall dialogs for any uninstalled apps.
  for (const AppPtr& delta : deltas) {
    if (delta->readiness != Readiness::kUnknown &&
        !apps_util::IsInstalled(delta->readiness) &&
        base::Contains(uninstall_dialogs_, delta->app_id)) {
      uninstall_dialogs_[delta->app_id]->CloseDialog();
    }
  }

  if (crosapi_subscriber_) {
    crosapi_subscriber_->OnApps(deltas, app_type, should_notify_initialized);
  }

  AppServiceProxyBase::OnApps(std::move(deltas), app_type,
                              should_notify_initialized);
}

void AppServiceProxyAsh::PauseApps(
    const std::map<std::string, PauseData>& pause_data) {
  for (auto& data : pause_data) {
    auto app_type = app_registry_cache_.GetAppType(data.first);
    if (app_type == AppType::kUnknown) {
      continue;
    }

    app_registry_cache_.ForOneApp(
        data.first, [this](const apps::AppUpdate& update) {
          if (!update.Paused().value_or(false)) {
            pending_pause_requests_.MaybeAddApp(update.AppId());
          }
        });

    // The app pause dialog can't be loaded for unit tests.
    if (!data.second.should_show_pause_dialog || is_using_testing_profile_) {
      auto* publisher = GetPublisher(app_type);
      if (publisher) {
        publisher->PauseApp(data.first);
      }
      continue;
    }

    app_registry_cache_.ForOneApp(
        data.first, [this, &data](const apps::AppUpdate& update) {
          LoadIconForDialog(
              update,
              base::BindOnce(&AppServiceProxyAsh::OnLoadIconForPauseDialog,
                             weak_ptr_factory_.GetWeakPtr(), update.AppType(),
                             update.AppId(), update.Name(), data.second));
        });
  }
}

void AppServiceProxyAsh::UnpauseApps(const std::set<std::string>& app_ids) {
  for (auto& app_id : app_ids) {
    auto app_type = app_registry_cache_.GetAppType(app_id);
    if (app_type == AppType::kUnknown) {
      continue;
    }

    pending_pause_requests_.MaybeRemoveApp(app_id);
    auto* publisher = GetPublisher(app_type);
    if (publisher) {
      publisher->UnpauseApp(app_id);
    }
  }
}

void AppServiceProxyAsh::SetResizeLocked(const std::string& app_id,
                                         bool locked) {
  auto* publisher = GetPublisher(app_registry_cache_.GetAppType(app_id));
  if (publisher) {
    publisher->SetResizeLocked(app_id, locked);
  }
}

void AppServiceProxyAsh::SetArcIsRegistered() {
  if (arc_is_registered_) {
    return;
  }

  arc_is_registered_ = true;
  if (publisher_host_) {
    publisher_host_->SetArcIsRegistered();
  }
}

void AppServiceProxyAsh::LaunchAppWithIntent(const std::string& app_id,
                                             int32_t event_flags,
                                             IntentPtr intent,
                                             LaunchSource launch_source,
                                             WindowInfoPtr window_info,
                                             LaunchCallback callback) {
  apps::IntentPtr intent_copy = intent->Clone();
  base::OnceCallback launch_callback = base::BindOnce(
      &AppServiceProxyAsh::LaunchAppWithIntentIfAllowed,
      weak_ptr_factory_.GetWeakPtr(), app_id, event_flags, std::move(intent),
      std::move(launch_source), std::move(window_info), std::move(callback));

  policy::DlpFilesControllerAsh* files_controller =
      policy::DlpFilesControllerAsh::GetForPrimaryProfile();
  if (files_controller) {
    auto app_found = app_registry_cache_.ForOneApp(
        app_id, [&files_controller, &intent_copy,
                 &launch_callback](const apps::AppUpdate& update) {
          files_controller->CheckIfLaunchAllowed(update, std::move(intent_copy),
                                                 std::move(launch_callback));
        });
    if (!app_found)
      std::move(launch_callback).Run(/*is_allowed=*/true);
  } else {
    std::move(launch_callback).Run(/*is_allowed=*/true);
  }
}

base::WeakPtr<AppServiceProxyAsh> AppServiceProxyAsh::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void AppServiceProxyAsh::ReInitializeCrostiniForTesting() {
  if (publisher_host_) {
    publisher_host_->ReInitializeCrostiniForTesting(this);  // IN-TEST
  }
}

void AppServiceProxyAsh::SetDialogCreatedCallbackForTesting(
    base::OnceClosure callback) {
  dialog_created_callback_ = std::move(callback);
}

void AppServiceProxyAsh::UninstallForTesting(
    const std::string& app_id,
    gfx::NativeWindow parent_window,
    OnUninstallForTestingCallback callback) {
  UninstallImpl(app_id, UninstallSource::kUnknown, parent_window,
                std::move(callback));
}

void AppServiceProxyAsh::SetAppPlatformMetricsServiceForTesting(
    std::unique_ptr<apps::AppPlatformMetricsService>
        app_platform_metrics_service) {
  app_platform_metrics_service_ = std::move(app_platform_metrics_service);
}

void AppServiceProxyAsh::RegisterPublishersForTesting() {
  if (publisher_host_) {
    publisher_host_->RegisterPublishersForTesting();
  }
}

void AppServiceProxyAsh::ReadIconsForTesting(AppType app_type,
                                             const std::string& app_id,
                                             int32_t size_in_dip,
                                             const IconKey& icon_key,
                                             IconType icon_type,
                                             LoadIconCallback callback) {
  ReadIcons(app_type, app_id, size_in_dip, icon_key.Clone(), icon_type,
            std::move(callback));
}

apps::PromiseAppRegistryCache* AppServiceProxyAsh::PromiseAppRegistryCache() {
  if (!promise_app_service_) {
    return nullptr;
  }
  return promise_app_service_->PromiseAppRegistryCache();
}

apps::PromiseAppService* AppServiceProxyAsh::PromiseAppService() {
  if (!promise_app_service_) {
    return nullptr;
  }
  return promise_app_service_.get();
}

void AppServiceProxyAsh::OnPromiseApp(PromiseAppPtr delta) {
  if (!promise_app_service_) {
    return;
  }
  promise_app_service_->OnPromiseApp(std::move(delta));
}

void AppServiceProxyAsh::LoadPromiseIcon(const PackageId& package_id,
                                         int32_t size_hint_in_dip,
                                         IconEffects icon_effects,
                                         apps::LoadIconCallback callback) {
  PromiseAppService()->LoadIcon(package_id, size_hint_in_dip, icon_effects,
                                std::move(callback));
}

void AppServiceProxyAsh::RegisterShortcutPublisher(
    AppType app_type,
    ShortcutPublisher* publisher) {
  shortcut_publishers_[app_type] = publisher;
}

apps::ShortcutRegistryCache* AppServiceProxyAsh::ShortcutRegistryCache() {
  return shortcut_registry_cache_ ? shortcut_registry_cache_.get() : nullptr;
}

void AppServiceProxyAsh::PublishShortcut(ShortcutPtr delta) {
  if (delta->icon_key.has_value() && delta->icon_key->raw_icon_updated) {
    MaybeScheduleIconFolderDeletionForShortcut(delta->shortcut_id);
  }
  ShortcutRegistryCache()->UpdateShortcut(std::move(delta));
}

void AppServiceProxyAsh::ShortcutRemoved(const ShortcutId& id) {
  MaybeScheduleIconFolderDeletionForShortcut(id);
  if (base::Contains(shortcut_removal_dialogs_, id)) {
    shortcut_removal_dialogs_[id]->CloseDialog();
  }
  ShortcutRegistryCache()->RemoveShortcut(id);
}

void AppServiceProxyAsh::LaunchShortcut(const ShortcutId& id,
                                        int64_t display_id) {
  std::string host_app_id = ShortcutRegistryCache()->GetShortcutHostAppId(id);
  std::string local_id = ShortcutRegistryCache()->GetShortcutLocalId(id);

  AppType app_type = AppRegistryCache().GetAppType(host_app_id);

  auto* shortcut_publisher = GetShortcutPublisher(app_type);
  if (!shortcut_publisher) {
    return;
  }
  shortcut_publisher->LaunchShortcut(host_app_id, local_id, display_id);

  // TODO(crbug.com/1412708): Add new launch source for shortcut and record
  // metrics.
  // TODO(crbug.com/1412708): Add callback to make launch async to support
  // Lacros.
}

void AppServiceProxyAsh::RemoveShortcut(const ShortcutId& id,
                                        UninstallSource uninstall_source,
                                        gfx::NativeWindow parent_window) {
  // If the dialog exists for the shortcut id, we bring the dialog to the front
  auto it = shortcut_removal_dialogs_.find(id);
  if (it != shortcut_removal_dialogs_.end()) {
    if (it->second->GetWidget()) {
      it->second->GetWidget()->Show();
    }
    return;
  }

  // Create the removal dialog object now so we can start tracking the parent
  // window.
  auto shortcut_removal_dialog_ptr = std::make_unique<ShortcutRemovalDialog>(
      profile_, id, parent_window,
      base::BindOnce(&AppServiceProxyAsh::OnShortcutRemovalDialogClosed,
                     weak_ptr_factory_.GetWeakPtr(), id, uninstall_source));
  ShortcutRemovalDialog* shortcut_removal_dialog =
      shortcut_removal_dialog_ptr.get();
  shortcut_removal_dialogs_.emplace(id, std::move(shortcut_removal_dialog_ptr));

  LoadShortcutIconWithBadge(
      id, apps::IconType::kStandard, kAppDialogIconSize,
      kAppDialogIconBadgeSize,
      /*allow_placeholder_icon = */ false,
      base::BindOnce(&AppServiceProxyAsh::OnLoadIconForShortcutRemovalDialog,
                     weak_ptr_factory_.GetWeakPtr(), id, uninstall_source,
                     parent_window, shortcut_removal_dialog));
}

void AppServiceProxyAsh::RemoveShortcutSilently(
    const ShortcutId& shortcut_id,
    UninstallSource uninstall_source) {
  RemoveShortcutImpl(shortcut_id, uninstall_source);
}

std::unique_ptr<IconLoader::Releaser> AppServiceProxyAsh::LoadShortcutIcon(
    const apps::ShortcutId& shortcut_id,
    const IconType& icon_type,
    int32_t size_hint_in_dip,
    bool allow_placeholder_icon,
    apps::LoadIconCallback callback) {
  if (!base::FeatureList::IsEnabled(features::kCrosWebAppShortcutUiUpdate)) {
    std::move(callback).Run(std::make_unique<IconValue>());
    return nullptr;
  }
  auto icon_key = shortcut_outer_icon_loader_.GetIconKey(shortcut_id.value());
  if (!icon_key.has_value()) {
    std::move(callback).Run(std::make_unique<IconValue>());
    return nullptr;
  }

  return shortcut_outer_icon_loader_.LoadIconFromIconKey(
      shortcut_id.value(), icon_key.value(), icon_type, size_hint_in_dip,
      allow_placeholder_icon, std::move(callback));
}

std::unique_ptr<IconLoader::Releaser>
AppServiceProxyAsh::LoadShortcutIconWithBadge(
    const apps::ShortcutId& shortcut_id,
    const IconType& icon_type,
    int32_t size_hint_in_dip,
    int32_t badge_size_hint_in_dip,
    bool allow_placeholder_icon,
    apps::LoadShortcutIconWithBadgeCallback callback) {
  if (!base::FeatureList::IsEnabled(features::kCrosWebAppShortcutUiUpdate)) {
    std::move(callback).Run(std::make_unique<IconValue>(),
                            std::make_unique<IconValue>());
    return nullptr;
  }
  return LoadShortcutIcon(
      shortcut_id, icon_type, size_hint_in_dip, allow_placeholder_icon,
      base::BindOnce(&AppServiceProxyAsh::OnShortcutIconLoaded,
                     weak_ptr_factory_.GetWeakPtr(), shortcut_id, icon_type,
                     badge_size_hint_in_dip, allow_placeholder_icon,
                     std::move(callback)));
}

apps::IconLoader* AppServiceProxyAsh::OverrideShortcutInnerIconLoaderForTesting(
    apps::IconLoader* icon_loader) {
  apps::IconLoader* old =
      shortcut_inner_icon_loader_.overriding_icon_loader_for_testing_;
  shortcut_inner_icon_loader_.overriding_icon_loader_for_testing_ = icon_loader;
  return old;
}

ShortcutPublisher* AppServiceProxyAsh::GetShortcutPublisherForTesting(
    AppType app_type) {
  return GetShortcutPublisher(app_type);
}

void AppServiceProxyAsh::LoadDefaultIcon(AppType app_type,
                                         int32_t size_in_dip,
                                         IconEffects icon_effects,
                                         IconType icon_type,
                                         LoadIconCallback callback) {
  auto* publisher = GetPublisher(app_type);
  int default_icon_resource_id = IDR_APP_DEFAULT_ICON;
  if (publisher) {
    default_icon_resource_id = publisher->DefaultIconResourceId();
  }
  LoadIconFromResource(
      profile_, absl::nullopt, icon_type, size_in_dip, default_icon_resource_id,
      /*is_placeholder_icon=*/false, icon_effects, std::move(callback));
}

AppServiceProxyAsh::ShortcutInnerIconLoader::ShortcutInnerIconLoader(
    AppServiceProxyAsh* host)
    : host_(host), overriding_icon_loader_for_testing_(nullptr) {}

absl::optional<IconKey> AppServiceProxyAsh::ShortcutInnerIconLoader::GetIconKey(
    const std::string& id) {
  if (overriding_icon_loader_for_testing_) {
    return overriding_icon_loader_for_testing_->GetIconKey(id);
  }

  if (!host_->ShortcutRegistryCache()->HasShortcut(ShortcutId(id))) {
    return absl::nullopt;
  }

  const absl::optional<IconKey>& icon_key =
      host_->ShortcutRegistryCache()->GetShortcut(ShortcutId(id))->icon_key;

  if (icon_key.has_value()) {
    return std::move(*icon_key->Clone());
  }

  return absl::nullopt;
}

std::unique_ptr<IconLoader::Releaser>
AppServiceProxyAsh::ShortcutInnerIconLoader::LoadIconFromIconKey(
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

  host_->ReadShortcutIcon(ShortcutId(id), size_hint_in_dip, icon_key.Clone(),
                          icon_type, std::move(callback));
  return nullptr;
}

void AppServiceProxyAsh::Shutdown() {
  crosapi_subscriber_ = nullptr;

  app_platform_metrics_service_.reset();

  uninstall_dialogs_.clear();

  if (publisher_host_) {
    publisher_host_->Shutdown();
  }
}

void AppServiceProxyAsh::UninstallImpl(const std::string& app_id,
                                       UninstallSource uninstall_source,
                                       gfx::NativeWindow parent_window,
                                       OnUninstallForTestingCallback callback) {
  // If the dialog exists for the app id, we bring the dialog to the front
  auto it = uninstall_dialogs_.find(app_id);
  if (it != uninstall_dialogs_.end()) {
    if (it->second->GetWidget()) {
      it->second->GetWidget()->Show();
    }
    if (!callback.is_null()) {
      std::move(callback).Run(false);
    }
    return;
  }

  app_registry_cache_.ForOneApp(app_id, [this, uninstall_source, parent_window,
                                         &callback](
                                            const apps::AppUpdate& update) {
    auto icon_key = update.IconKey();
    DCHECK(icon_key.has_value());
    auto app_type = update.AppType();
    auto uninstall_dialog_ptr = std::make_unique<UninstallDialog>(
        profile_, app_type, update.AppId(), update.Name(), parent_window,
        base::BindOnce(&AppServiceProxyAsh::OnUninstallDialogClosed,
                       weak_ptr_factory_.GetWeakPtr(), app_type, update.AppId(),
                       uninstall_source));
    UninstallDialog* uninstall_dialog = uninstall_dialog_ptr.get();
    uninstall_dialog_ptr->SetDialogCreatedCallbackForTesting(
        std::move(callback));
    uninstall_dialogs_.emplace(update.AppId(), std::move(uninstall_dialog_ptr));
    uninstall_dialog->PrepareToShow(std::move(icon_key.value()),
                                    this->app_icon_loader(),
                                    kAppDialogIconSize);
  });
}

void AppServiceProxyAsh::OnUninstallDialogClosed(
    apps::AppType app_type,
    const std::string& app_id,
    UninstallSource uninstall_source,
    bool uninstall,
    bool clear_site_data,
    bool report_abuse,
    UninstallDialog* uninstall_dialog) {
  if (uninstall) {
    auto* publisher = GetPublisher(app_type);
    DCHECK(publisher);
    publisher->Uninstall(app_id, uninstall_source, clear_site_data,
                         report_abuse);

    PerformPostUninstallTasks(app_type, app_id, uninstall_source);
  }

  DCHECK(uninstall_dialog);
  auto it = uninstall_dialogs_.find(app_id);
  DCHECK(it != uninstall_dialogs_.end());
  uninstall_dialogs_.erase(it);
}

void AppServiceProxyAsh::OnShortcutRemovalDialogClosed(
    const ShortcutId& shortcut_id,
    UninstallSource uninstall_source,
    bool remove,
    ShortcutRemovalDialog* shortcut_removal_dialog) {
  if (remove) {
    RemoveShortcutImpl(shortcut_id, uninstall_source);
  }
  CHECK(shortcut_removal_dialog);
  auto it = shortcut_removal_dialogs_.find(shortcut_id);
  CHECK(it != shortcut_removal_dialogs_.end());
  shortcut_removal_dialogs_.erase(it);
}

void AppServiceProxyAsh::OnLoadIconForShortcutRemovalDialog(
    const ShortcutId& id,
    UninstallSource uninstall_source,
    gfx::NativeWindow parent_window,
    ShortcutRemovalDialog* shortcut_removal_dialog,
    IconValuePtr icon_value,
    IconValuePtr badge_icon_value) {
  if (icon_value && badge_icon_value &&
      icon_value->icon_type == IconType::kStandard &&
      badge_icon_value->icon_type == IconType::kStandard) {
    shortcut_removal_dialog->CreateDialog(icon_value->uncompressed,
                                          badge_icon_value->uncompressed);
    return;
  }

  // If the icon loaded is not valid, call the callback to clean up the
  // `shortcut_removal_dialogs_` map.
  shortcut_removal_dialog->OnDialogClosed(false);
}

void AppServiceProxyAsh::InitializePreferredAppsForAllSubscribers() {
  AppServiceProxyBase::InitializePreferredAppsForAllSubscribers();
  if (crosapi_subscriber_ && preferred_apps_impl_) {
    crosapi_subscriber_->InitializePreferredApps(
        preferred_apps_impl_->preferred_apps_list().GetValue());
  }
}

void AppServiceProxyAsh::OnPreferredAppsChanged(
    PreferredAppChangesPtr changes) {
  if (!crosapi_subscriber_) {
    AppServiceProxyBase::OnPreferredAppsChanged(std::move(changes));
    return;
  }

  DCHECK(changes);
  AppServiceProxyBase::OnPreferredAppsChanged(changes->Clone());
  crosapi_subscriber_->OnPreferredAppsChanged(std::move(changes));
}

bool AppServiceProxyAsh::MaybeShowLaunchPreventionDialog(
    const apps::AppUpdate& update) {
  if (update.AppId() == app_constants::kChromeAppId) {
    return false;
  }

  // Return true, and load the icon for the app block dialog when the app
  // is blocked by policy.
  if (update.Readiness() == apps::Readiness::kDisabledByPolicy) {
    LoadIconForDialog(
        update, base::BindOnce(&AppServiceProxyAsh::OnLoadIconForBlockDialog,
                               weak_ptr_factory_.GetWeakPtr(), update.Name()));
    return true;
  }

  // Return true, and load the icon for the app pause dialog when the app
  // is paused.
  if (update.Paused().value_or(false) ||
      pending_pause_requests_.IsPaused(update.AppId())) {
    ash::app_time::AppTimeLimitInterface* app_limit =
        ash::app_time::AppTimeLimitInterface::Get(profile_);
    DCHECK(app_limit);
    auto time_limit =
        app_limit->GetTimeLimitForApp(update.AppId(), update.AppType());
    if (!time_limit.has_value()) {
      NOTREACHED();
      return true;
    }
    PauseData pause_data;
    pause_data.hours = time_limit.value().InHours();
    pause_data.minutes = time_limit.value().InMinutes() % 60;
    LoadIconForDialog(
        update, base::BindOnce(&AppServiceProxyAsh::OnLoadIconForPauseDialog,
                               weak_ptr_factory_.GetWeakPtr(), update.AppType(),
                               update.AppId(), update.Name(), pause_data));
    return true;
  }

  // The app is not prevented from launching and we didn't show any dialog.
  return false;
}

void AppServiceProxyAsh::OnLaunched(LaunchCallback callback,
                                    LaunchResult&& launch_result) {
  base::RepeatingCallback<bool(void)> ready_to_run_callback =
      base::BindRepeating(&AppServiceProxyAsh::CanRunLaunchCallback,
                          base::Unretained(this), launch_result.instance_ids);
  base::OnceClosure launch_callback =
      base::BindOnce(std::move(callback), std::move(launch_result));
  if (ready_to_run_callback.Run()) {
    std::move(launch_callback).Run();
  } else {
    callback_list_.emplace_back(
        std::make_pair(ready_to_run_callback, std::move(launch_callback)));
  }
}

void AppServiceProxyAsh::LoadIconForDialog(const apps::AppUpdate& update,
                                           apps::LoadIconCallback callback) {
  constexpr bool kAllowPlaceholderIcon = false;
  auto icon_type = IconType::kStandard;

  // For browser tests, load the app icon, because there is no family link
  // logo for browser tests.
  //
  // For non_child profile, load the app icon, because the app is blocked by
  // admin.
  if (!dialog_created_callback_.is_null() || !profile_->IsChild()) {
    LoadIcon(update.AppType(), update.AppId(), icon_type, kAppDialogIconSize,
             kAllowPlaceholderIcon, std::move(callback));
    return;
  }

  // Load the family link kite logo icon for the app pause dialog or the app
  // block dialog for the child profile.
  LoadIconFromResource(/*profile=*/nullptr, /*app_id=*/absl::nullopt, icon_type,
                       kAppDialogIconSize, IDR_SUPERVISED_USER_ICON,
                       kAllowPlaceholderIcon, IconEffects::kNone,
                       std::move(callback));
}

void AppServiceProxyAsh::OnLoadIconForBlockDialog(const std::string& app_name,
                                                  IconValuePtr icon_value) {
  if (icon_value->icon_type != IconType::kStandard) {
    return;
  }

  AppServiceProxyAsh::CreateBlockDialog(app_name, icon_value->uncompressed,
                                        profile_);

  // For browser tests, call the dialog created callback to stop the run loop.
  if (!dialog_created_callback_.is_null()) {
    std::move(dialog_created_callback_).Run();
  }
}

void AppServiceProxyAsh::OnLoadIconForPauseDialog(apps::AppType app_type,
                                                  const std::string& app_id,
                                                  const std::string& app_name,
                                                  const PauseData& pause_data,
                                                  IconValuePtr icon_value) {
  if (icon_value->icon_type != IconType::kStandard) {
    OnPauseDialogClosed(app_type, app_id);
    return;
  }

  AppServiceProxyAsh::CreatePauseDialog(
      app_type, app_name, icon_value->uncompressed, pause_data,
      base::BindOnce(&AppServiceProxyAsh::OnPauseDialogClosed,
                     weak_ptr_factory_.GetWeakPtr(), app_type, app_id));

  // For browser tests, call the dialog created callback to stop the run loop.
  if (!dialog_created_callback_.is_null()) {
    std::move(dialog_created_callback_).Run();
  }
}

void AppServiceProxyAsh::OnPauseDialogClosed(apps::AppType app_type,
                                             const std::string& app_id) {
  bool should_pause_app = pending_pause_requests_.IsPaused(app_id);
  if (!should_pause_app) {
    app_registry_cache_.ForOneApp(
        app_id, [&should_pause_app](const apps::AppUpdate& update) {
          if (update.Paused().value_or(false)) {
            should_pause_app = true;
          }
        });
  }
  if (should_pause_app) {
    auto* publisher = GetPublisher(app_type);
    if (publisher) {
      publisher->PauseApp(app_id);
    }
  }
}

void AppServiceProxyAsh::OnAppUpdate(const apps::AppUpdate& update) {
  if ((update.PausedChanged() && update.Paused().value_or(false)) ||
      (update.ReadinessChanged() &&
       !apps_util::IsInstalled(update.Readiness()))) {
    pending_pause_requests_.MaybeRemoveApp(update.AppId());
  }
}

void AppServiceProxyAsh::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  app_registry_cache_observer_.Reset();
}

void AppServiceProxyAsh::RecordAppPlatformMetrics(
    Profile* profile,
    const apps::AppUpdate& update,
    apps::LaunchSource launch_source,
    apps::LaunchContainer container) {
  RecordAppLaunchMetrics(profile, update.AppType(), update.AppId(),
                         launch_source, container);
}

void AppServiceProxyAsh::InitAppPlatformMetrics() {
  if (app_platform_metrics_service_) {
    app_platform_metrics_service_->Start(app_registry_cache_,
                                         instance_registry_);
  }
}

void AppServiceProxyAsh::PerformPostUninstallTasks(
    apps::AppType app_type,
    const std::string& app_id,
    UninstallSource uninstall_source) {
  if (app_platform_metrics_service_ &&
      app_platform_metrics_service_->AppPlatformMetrics()) {
    app_platform_metrics_service_->AppPlatformMetrics()->RecordAppUninstallUkm(
        app_type, app_id, uninstall_source);
  }
}

void AppServiceProxyAsh::PerformPostLaunchTasks(
    apps::LaunchSource launch_source) {
  if (apps_util::IsHumanLaunch(launch_source)) {
    ash::full_restore::FullRestoreService::MaybeCloseNotification(profile_);
  }
}

void AppServiceProxyAsh::OnInstanceUpdate(const apps::InstanceUpdate& update) {
  if (!update.IsCreation()) {
    return;
  }

  callback_list_.remove_if([](std::pair<base::RepeatingCallback<bool(void)>,
                                        base::OnceClosure>& callbacks) {
    if (callbacks.first.Run()) {
      std::move(callbacks.second).Run();
      return true;
    }
    return false;
  });
}

void AppServiceProxyAsh::OnInstanceRegistryWillBeDestroyed(
    apps::InstanceRegistry* cache) {
  instance_registry_observer_.Reset();
}

bool AppServiceProxyAsh::CanRunLaunchCallback(
    const std::vector<base::UnguessableToken>& instance_ids) {
  for (const base::UnguessableToken& instance_id : instance_ids) {
    bool exists = false;
    InstanceRegistry().ForOneInstance(
        instance_id,
        [&exists](const apps::InstanceUpdate& update) { exists = true; });
    if (!exists)
      return false;
  }

  return true;
}

void AppServiceProxyAsh::LaunchAppWithIntentIfAllowed(
    const std::string& app_id,
    int32_t event_flags,
    IntentPtr intent,
    LaunchSource launch_source,
    WindowInfoPtr window_info,
    LaunchCallback callback,
    bool is_allowed) {
  if (!is_allowed) {
    std::move(callback).Run(LaunchResult(State::FAILED));
    return;
  }
  AppServiceProxyBase::LaunchAppWithIntent(
      app_id, event_flags, std::move(intent), std::move(launch_source),
      std::move(window_info), std::move(callback));
}

bool AppServiceProxyAsh::ShouldReadIcons(AppType app_type) {
  // Exclude the remote apps, because remote apps regenerate app id for each
  // user login session. So we can't save the remote app icon image files in the
  // app id directories.
  return app_type != AppType::kRemote;
}

void AppServiceProxyAsh::ReadIcons(AppType app_type,
                                   const std::string& app_id,
                                   int32_t size_in_dip,
                                   std::unique_ptr<IconKey> icon_key,
                                   IconType icon_type,
                                   LoadIconCallback callback) {
  auto it = pending_read_icon_requests_.find(app_id);
  if (it != pending_read_icon_requests_.end()) {
    // The icon folder is being deleted, so add the `ReadIcons` request to
    // `pending_read_icon_requests_` to wait for the deletion.
    it->second.push_back(base::BindOnce(
        &AppServiceProxyAsh::ReadIcons, weak_ptr_factory_.GetWeakPtr(),
        app_type, app_id, size_in_dip, std::move(icon_key), icon_type,
        std::move(callback)));
    return;
  }

  icon_reader_.ReadIcons(
      app_id, size_in_dip, *icon_key, icon_type,
      base::BindOnce(&AppServiceProxyAsh::OnIconRead,
                     weak_ptr_factory_.GetWeakPtr(), app_type, app_id,
                     size_in_dip,
                     static_cast<IconEffects>(icon_key->icon_effects),
                     icon_type, std::move(callback)));
}

void AppServiceProxyAsh::OnIconRead(AppType app_type,
                                    const std::string& app_id,
                                    int32_t size_in_dip,
                                    IconEffects icon_effects,
                                    IconType icon_type,
                                    LoadIconCallback callback,
                                    IconValuePtr iv) {
  if (!iv || (iv->uncompressed.isNull() && iv->compressed.empty())) {
    auto* publisher = GetPublisher(app_type);
    if (!publisher) {
      LOG(WARNING) << "No publisher for requested icon";
      LoadIconFromResource(
          profile_, app_id, icon_type, size_in_dip, IDR_APP_DEFAULT_ICON,
          /*is_placeholder_icon=*/false, icon_effects, std::move(callback));
      return;
    }

    icon_writer_.InstallIcon(
        publisher, app_id, size_in_dip,
        base::BindOnce(&AppServiceProxyAsh::OnIconInstalled,
                       weak_ptr_factory_.GetWeakPtr(), app_type, app_id,
                       size_in_dip, icon_effects, icon_type,
                       publisher->DefaultIconResourceId(),
                       std::move(callback)));
    return;
  }

  std::move(callback).Run(std::move(iv));
}

void AppServiceProxyAsh::OnIconInstalled(AppType app_type,
                                         const std::string& app_id,
                                         int32_t size_in_dip,
                                         IconEffects icon_effects,
                                         IconType icon_type,
                                         int default_icon_resource_id,
                                         LoadIconCallback callback,
                                         bool install_success) {
  if (!install_success) {
    LoadIconFromResource(
        profile_, app_id, icon_type, size_in_dip, default_icon_resource_id,
        /*is_placeholder_icon=*/false, icon_effects, std::move(callback));
    return;
  }

  IconKey icon_key;
  icon_key.icon_effects = icon_effects;
  icon_reader_.ReadIcons(app_id, size_in_dip, icon_key, icon_type,
                         std::move(callback));
}

void AppServiceProxyAsh::PostIconFoldersDeletion(
    const std::vector<std::string>& ids) {
  for (const auto& id : ids) {
    auto it = pending_read_icon_requests_.find(id);
    if (it == pending_read_icon_requests_.end()) {
      continue;
    }

    // The saved `ReadIcons` requests in `pending_read_icon_requests_` are run
    // to load the icon for `app_id`.
    std::vector<base::OnceCallback<void()>> callbacks = std::move(it->second);
    pending_read_icon_requests_.erase(it);
    for (auto& callback : callbacks) {
      std::move(callback).Run();
    }
  }
}

IntentLaunchInfo AppServiceProxyAsh::CreateIntentLaunchInfo(
    const apps::IntentPtr& intent,
    const apps::IntentFilterPtr& filter,
    const apps::AppUpdate& update) {
  IntentLaunchInfo entry =
      AppServiceProxyBase::CreateIntentLaunchInfo(intent, filter, update);
  if (policy::DlpFilesControllerAsh* files_controller =
          policy::DlpFilesControllerAsh::GetForPrimaryProfile()) {
    entry.is_dlp_blocked = files_controller->IsLaunchBlocked(update, intent);
  }
  return entry;
}

ShortcutPublisher* AppServiceProxyAsh::GetShortcutPublisher(AppType app_type) {
  auto it = shortcut_publishers_.find(app_type);
  return it == shortcut_publishers_.end() ? nullptr : it->second;
}

void AppServiceProxyAsh::OnShortcutIconLoaded(
    const ShortcutId& shortcut_id,
    const IconType& icon_type,
    int32_t badge_size_hint_in_dip,
    bool allow_placeholder_icon,
    apps::LoadShortcutIconWithBadgeCallback callback,
    IconValuePtr shortcut_icon) {
  std::string host_app_id =
      ShortcutRegistryCache()->GetShortcutHostAppId(shortcut_id);
  AppType app_type = AppRegistryCache().GetAppType(host_app_id);
  LoadIcon(app_type, host_app_id, icon_type, badge_size_hint_in_dip,
           allow_placeholder_icon,
           base::BindOnce(&AppServiceProxyAsh::OnHostAppIconForShortcutLoaded,
                          weak_ptr_factory_.GetWeakPtr(),
                          std::move(shortcut_icon), std::move(callback)));
}

void AppServiceProxyAsh::OnHostAppIconForShortcutLoaded(
    IconValuePtr shortcut_icon,
    apps::LoadShortcutIconWithBadgeCallback callback,
    IconValuePtr host_app_icon) {
  std::move(callback).Run(std::move(shortcut_icon), std::move(host_app_icon));
}

void AppServiceProxyAsh::RemoveShortcutImpl(const ShortcutId& shortcut_id,
                                            UninstallSource uninstall_source) {
  std::string host_app_id =
      ShortcutRegistryCache()->GetShortcutHostAppId(shortcut_id);
  std::string local_id =
      ShortcutRegistryCache()->GetShortcutLocalId(shortcut_id);
  AppType app_type = AppRegistryCache().GetAppType(host_app_id);

  auto* shortcut_publisher = GetShortcutPublisher(app_type);
  if (shortcut_publisher) {
    shortcut_publisher->RemoveShortcut(host_app_id, local_id, uninstall_source);
  }
}

void AppServiceProxyAsh::ReadShortcutIcon(const ShortcutId& shortcut_id,
                                          int32_t size_in_dip,
                                          std::unique_ptr<IconKey> icon_key,
                                          IconType icon_type,
                                          LoadIconCallback callback) {
  auto it = pending_read_icon_requests_.find(shortcut_id.value());
  if (it != pending_read_icon_requests_.end()) {
    // The icon folder is being deleted, so add the `ReadShortcutIcon` request
    // to `pending_read_icon_requests_` to wait for the deletion.
    it->second.push_back(
        base::BindOnce(&AppServiceProxyAsh::ReadShortcutIcon,
                       weak_ptr_factory_.GetWeakPtr(), shortcut_id, size_in_dip,
                       std::move(icon_key), icon_type, std::move(callback)));
    return;
  }
  icon_reader_.ReadIcons(
      shortcut_id.value(), size_in_dip, *icon_key, icon_type,
      base::BindOnce(&AppServiceProxyAsh::OnShortcutIconRead,
                     weak_ptr_factory_.GetWeakPtr(), shortcut_id, size_in_dip,
                     static_cast<IconEffects>(icon_key->icon_effects),
                     icon_type, std::move(callback)));
}

void AppServiceProxyAsh::OnShortcutIconRead(const ShortcutId& shortcut_id,
                                            int32_t size_in_dip,
                                            IconEffects icon_effects,
                                            IconType icon_type,
                                            LoadIconCallback callback,
                                            IconValuePtr iv) {
  if (!iv || (iv->uncompressed.isNull() && iv->compressed.empty())) {
    std::string host_app_id =
        ShortcutRegistryCache()->GetShortcutHostAppId(shortcut_id);
    AppType app_type = AppRegistryCache().GetAppType(host_app_id);
    auto* publisher = GetShortcutPublisher(app_type);
    if (!publisher) {
      LOG(WARNING) << "No publisher for requested icon";
      LoadIconFromResource(
          profile_, absl::nullopt, icon_type, size_in_dip, IDR_APP_DEFAULT_ICON,
          /*is_placeholder_icon=*/false, icon_effects, std::move(callback));
      return;
    }
    icon_writer_.InstallIcon(
        publisher, shortcut_id.value(), size_in_dip,
        base::BindOnce(&AppServiceProxyAsh::OnShortcutIconInstalled,
                       weak_ptr_factory_.GetWeakPtr(), shortcut_id, size_in_dip,
                       icon_effects, icon_type, IDR_APP_DEFAULT_ICON,
                       std::move(callback)));
    return;
  }

  std::move(callback).Run(std::move(iv));
}

void AppServiceProxyAsh::OnShortcutIconInstalled(const ShortcutId& shortcut_id,
                                                 int32_t size_in_dip,
                                                 IconEffects icon_effects,
                                                 IconType icon_type,
                                                 int default_icon_resource_id,
                                                 LoadIconCallback callback,
                                                 bool install_success) {
  if (!install_success) {
    LoadIconFromResource(profile_, absl::nullopt, icon_type, size_in_dip,
                         default_icon_resource_id,
                         /*is_placeholder_icon=*/false, icon_effects,
                         std::move(callback));
    return;
  }
  IconKey icon_key;
  icon_key.icon_effects = icon_effects;
  icon_reader_.ReadIcons(shortcut_id.value(), size_in_dip, icon_key, icon_type,
                         std::move(callback));
}

void AppServiceProxyAsh::MaybeScheduleIconFolderDeletionForShortcut(
    const ShortcutId& shortcut_id) {
  if (!base::Contains(pending_read_icon_requests_, shortcut_id.value())) {
    pending_read_icon_requests_[shortcut_id.value()] =
        std::vector<base::OnceCallback<void()>>();
    std::vector<std::string> shortcut_ids({shortcut_id.value()});
    ScheduleIconFoldersDeletion(
        profile_->GetPath(), shortcut_ids,
        base::BindOnce(&AppServiceProxyAsh::PostIconFoldersDeletion,
                       weak_ptr_factory_.GetWeakPtr(), shortcut_ids));
  }
}

}  // namespace apps
