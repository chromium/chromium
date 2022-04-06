// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_service_proxy_ash.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/apps/app_service/browser_app_instance_registry.h"
#include "chrome/browser/apps/app_service/browser_app_instance_tracker.h"
#include "chrome/browser/apps/app_service/instance_registry_updater.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics_service.h"
#include "chrome/browser/apps/app_service/metrics/app_service_metrics.h"
#include "chrome/browser/apps/app_service/uninstall_dialog.h"
#include "chrome/browser/ash/app_restore/full_restore_service.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_time_limit_interface.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/grit/supervised_user_unscaled_resources.h"
#include "chrome/browser/web_applications/app_service/web_apps.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "components/account_id/account_id.h"
#include "components/app_constants/constants.h"
#include "components/app_restore/full_restore_save_handler.h"
#include "components/app_restore/full_restore_utils.h"
#include "components/services/app_service/app_service_mojom_impl.h"
#include "components/services/app_service/public/cpp/app_capability_access_cache_wrapper.h"
#include "components/services/app_service/public/cpp/app_registry_cache_wrapper.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "components/user_manager/user.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace apps {

AppServiceProxyAsh::AppServiceProxyAsh(Profile* profile)
    : AppServiceProxyBase(profile) {
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

void AppServiceProxyAsh::Initialize() {
  if (!IsValidProfile()) {
    return;
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

  if (!app_service_.is_connected()) {
    return;
  }

  AppRegistryCache::Observer::Observe(&AppRegistryCache());

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
  if (!profile_->AsTestingProfile()) {
    app_platform_metrics_service_ =
        std::make_unique<AppPlatformMetricsService>(profile_);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&AppServiceProxyAsh::InitAppPlatformMetrics,
                                  weak_ptr_factory_.GetWeakPtr()));
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

apps::BrowserAppInstanceTracker*
AppServiceProxyAsh::BrowserAppInstanceTracker() {
  return browser_app_instance_tracker_.get();
}

apps::BrowserAppInstanceRegistry*
AppServiceProxyAsh::BrowserAppInstanceRegistry() {
  return browser_app_instance_registry_.get();
}

void AppServiceProxyAsh::RegisterCrosApiSubScriber(
    SubscriberCrosapi* subscriber) {
  if (base::FeatureList::IsEnabled(AppServiceCrosApiOnAppsWithoutMojom)) {
    crosapi_subscriber_ = subscriber;
    crosapi_subscriber_->OnApps(app_registry_cache_.GetAllApps());
  }
}

void AppServiceProxyAsh::Uninstall(
    const std::string& app_id,
    apps::mojom::UninstallSource uninstall_source,
    gfx::NativeWindow parent_window) {
  UninstallImpl(app_id, uninstall_source, parent_window, base::DoNothing());
}

void AppServiceProxyAsh::OnApps(std::vector<AppPtr> deltas,
                                AppType app_type,
                                bool should_notify_initialized) {
  if (crosapi_subscriber_) {
    crosapi_subscriber_->OnApps(deltas);
  }

  AppServiceProxyBase::OnApps(std::move(deltas), app_type,
                              should_notify_initialized);
}

void AppServiceProxyAsh::OnApps(std::vector<apps::mojom::AppPtr> deltas,
                                apps::mojom::AppType app_type,
                                bool should_notify_initialized) {
  AppServiceProxyBase::OnApps(std::move(deltas), app_type,
                              should_notify_initialized);
}

void AppServiceProxyAsh::PauseApps(
    const std::map<std::string, PauseData>& pause_data) {
  if (!app_service_.is_connected()) {
    return;
  }

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
      app_service_->PauseApp(ConvertAppTypeToMojomAppType(app_type),
                             data.first);
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
  if (!app_service_.is_connected()) {
    return;
  }

  for (auto& app_id : app_ids) {
    auto app_type = app_registry_cache_.GetAppType(app_id);
    if (app_type == AppType::kUnknown) {
      continue;
    }

    pending_pause_requests_.MaybeRemoveApp(app_id);
    app_service_->UnpauseApp(ConvertAppTypeToMojomAppType(app_type), app_id);
  }
}

void AppServiceProxyAsh::SetResizeLocked(const std::string& app_id,
                                         apps::mojom::OptionalBool locked) {
  if (app_service_.is_connected()) {
    auto app_type = app_registry_cache_.GetAppType(app_id);
    app_service_->SetResizeLocked(ConvertAppTypeToMojomAppType(app_type),
                                  app_id, locked);
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

void AppServiceProxyAsh::FlushMojoCallsForTesting() {
  app_service_mojom_impl_->FlushMojoCallsForTesting();

  if (publisher_host_) {
    publisher_host_->FlushMojoCallsForTesting();
  }

  receivers_.FlushForTesting();
}

void AppServiceProxyAsh::ReInitializeCrostiniForTesting() {
  if (app_service_.is_connected() && publisher_host_) {
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
  UninstallImpl(app_id, apps::mojom::UninstallSource::kUnknown, parent_window,
                std::move(callback));
}

void AppServiceProxyAsh::SetAppPlatformMetricsServiceForTesting(
    std::unique_ptr<apps::AppPlatformMetricsService>
        app_platform_metrics_service) {
  app_platform_metrics_service_ = std::move(app_platform_metrics_service);
}

void AppServiceProxyAsh::Shutdown() {
  crosapi_subscriber_ = nullptr;

  app_platform_metrics_service_.reset();

  uninstall_dialogs_.clear();

  if (publisher_host_) {
    publisher_host_->Shutdown();
  }
}

void AppServiceProxyAsh::UninstallImpl(
    const std::string& app_id,
    apps::mojom::UninstallSource uninstall_source,
    gfx::NativeWindow parent_window,
    OnUninstallForTestingCallback callback) {
  if (!app_service_.is_connected()) {
    if (!callback.is_null()) {
      std::move(callback).Run(false);
    }
    return;
  }

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
    uninstall_dialog->PrepareToShow(std::move(icon_key.value()), this);
  });
}

void AppServiceProxyAsh::OnUninstallDialogClosed(
    apps::AppType app_type,
    const std::string& app_id,
    apps::mojom::UninstallSource uninstall_source,
    bool uninstall,
    bool clear_site_data,
    bool report_abuse,
    UninstallDialog* uninstall_dialog) {
  if (uninstall) {
    app_registry_cache_.ForOneApp(app_id, RecordAppBounce);

    app_service_->Uninstall(ConvertAppTypeToMojomAppType(app_type), app_id,
                            uninstall_source, clear_site_data, report_abuse);

    PerformPostUninstallTasks(app_type, app_id, uninstall_source);
  }

  DCHECK(uninstall_dialog);
  auto it = uninstall_dialogs_.find(app_id);
  DCHECK(it != uninstall_dialogs_.end());
  uninstall_dialogs_.erase(it);
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
  auto icon_key = update.IconKey();
  constexpr bool kAllowPlaceholderIcon = false;
  constexpr int32_t kIconSize = 48;
  auto app_type = update.AppType();
  auto icon_type = IconType::kStandard;

  // For browser tests, load the app icon, because there is no family link
  // logo for browser tests.
  //
  // For non_child profile, load the app icon, because the app is blocked by
  // admin.
  if (!dialog_created_callback_.is_null() || !profile_->IsChild()) {
    if (base::FeatureList::IsEnabled(
            features::kAppServiceLoadIconWithoutMojom)) {
      if (!icon_key.has_value()) {
        std::move(callback).Run(std::make_unique<IconValue>());
        return;
      }
      LoadIconFromIconKey(app_type, update.AppId(), icon_key.value(), icon_type,
                          kIconSize, kAllowPlaceholderIcon,
                          std::move(callback));
    } else {
      if (!icon_key.has_value()) {
        MojomIconValueToIconValueCallback(std::move(callback))
            .Run(apps::mojom::IconValue::New());
        return;
      }
      LoadIconFromIconKey(
          ConvertAppTypeToMojomAppType(app_type), update.AppId(),
          ConvertIconKeyToMojomIconKey(icon_key.value()),
          apps::mojom::IconType::kStandard, kIconSize, kAllowPlaceholderIcon,
          MojomIconValueToIconValueCallback(std::move(callback)));
    }
    return;
  }

  // Load the family link kite logo icon for the app pause dialog or the app
  // block dialog for the child profile.
  LoadIconFromResource(icon_type, kIconSize, IDR_SUPERVISED_USER_ICON,
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
    app_service_->PauseApp(ConvertAppTypeToMojomAppType(app_type), app_id);
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
  AppRegistryCache::Observer::Observe(nullptr);
}

void AppServiceProxyAsh::RecordAppPlatformMetrics(
    Profile* profile,
    const apps::AppUpdate& update,
    apps::mojom::LaunchSource launch_source,
    apps::mojom::LaunchContainer container) {
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
    apps::mojom::UninstallSource uninstall_source) {
  if (app_platform_metrics_service_ &&
      app_platform_metrics_service_->AppPlatformMetrics()) {
    app_platform_metrics_service_->AppPlatformMetrics()->RecordAppUninstallUkm(
        app_type, app_id, uninstall_source);
  }
}

void AppServiceProxyAsh::PerformPostLaunchTasks(
    apps::mojom::LaunchSource launch_source) {
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

}  // namespace apps
