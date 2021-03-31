// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_service_proxy_chromeos.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "chrome/browser/apps/app_service/app_service_metrics.h"
#include "chrome/browser/apps/app_service/publishers/lacros_apps.h"
#include "chrome/browser/apps/app_service/uninstall_dialog.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_time_limit_interface.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/grit/supervised_user_unscaled_resources.h"
#include "chrome/common/chrome_features.h"
#include "components/account_id/account_id.h"
#include "components/services/app_service/app_service_impl.h"
#include "components/services/app_service/public/cpp/app_capability_access_cache_wrapper.h"
#include "components/services/app_service/public/cpp/app_registry_cache_wrapper.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "components/user_manager/user.h"
#include "extensions/common/constants.h"

namespace apps {

namespace {

bool g_omit_built_in_apps_for_testing_ = false;
bool g_omit_plugin_vm_apps_for_testing_ = false;

}  // anonymous namespace

AppServiceProxyChromeOs::AppServiceProxyChromeOs(Profile* profile)
    : AppServiceProxyBase(profile) {
  Initialize();
}

AppServiceProxyChromeOs::~AppServiceProxyChromeOs() {
  AppCapabilityAccessCacheWrapper::Get().RemoveAppCapabilityAccessCache(
      &app_capability_access_cache_);
  AppRegistryCacheWrapper::Get().RemoveAppRegistryCache(&app_registry_cache_);
}

void AppServiceProxyChromeOs::Initialize() {
  if (!IsValidProfile()) {
    return;
  }

  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile_);
  if (user) {
    const AccountId& account_id = user->GetAccountId();
    app_registry_cache_.SetAccountId(account_id);
    AppRegistryCacheWrapper::Get().AddAppRegistryCache(account_id,
                                                       &app_registry_cache_);
    app_capability_access_cache_.SetAccountId(account_id);
    AppCapabilityAccessCacheWrapper::Get().AddAppCapabilityAccessCache(
        account_id, &app_capability_access_cache_);
  }

  AppServiceProxyBase::Initialize();

  if (!app_service_.is_connected()) {
    return;
  }

  // The AppServiceProxy is also a publisher, of a variety of app types. That
  // responsibility isn't intrinsically part of the AppServiceProxy, but doing
  // that here, for each such app type, is as good a place as any.
  if (!g_omit_built_in_apps_for_testing_) {
    built_in_chrome_os_apps_ =
        std::make_unique<BuiltInChromeOsApps>(app_service_, profile_);
  }
  // TODO(b/170591339): Allow borealis to provide apps for the non-primary
  // profile.
  if (guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile_)) {
    borealis_apps_ = std::make_unique<BorealisApps>(app_service_, profile_);
  }
  crostini_apps_ = std::make_unique<CrostiniApps>(app_service_, profile_);
  extension_apps_ = std::make_unique<ExtensionAppsChromeOs>(
      app_service_, profile_, &instance_registry_);
  if (!g_omit_plugin_vm_apps_for_testing_) {
    plugin_vm_apps_ = std::make_unique<PluginVmApps>(app_service_, profile_);
  }
  // Lacros does not support multi-signin, so only create for the primary
  // profile. This also avoids creating an instance for the lock screen app
  // profile and ensures there is only one instance of LacrosApps.
  if (crosapi::browser_util::IsLacrosEnabled() &&
      chromeos::ProfileHelper::IsPrimaryProfile(profile_)) {
    lacros_apps_ = std::make_unique<LacrosApps>(app_service_);
  }
  web_apps_ = std::make_unique<WebAppsChromeOs>(app_service_, profile_,
                                                &instance_registry_);

  // After moving the web apps to Lacros, the current web app publisher
  // will become System web app publisher, and a lacros web app publisher
  // needs to be created.
  if (crosapi::browser_util::IsLacrosEnabled() &&
      chromeos::ProfileHelper::IsPrimaryProfile(profile_) &&
      base::FeatureList::IsEnabled(features::kLacrosWebApps)) {
    lacros_web_apps_ = std::make_unique<LacrosWebApps>(app_service_);
  }

  // Asynchronously add app icon source, so we don't do too much work in the
  // constructor.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&AppServiceProxyChromeOs::AddAppIconSource,
                                weak_ptr_factory_.GetWeakPtr(), profile_));
}

apps::InstanceRegistry& AppServiceProxyChromeOs::InstanceRegistry() {
  return instance_registry_;
}

void AppServiceProxyChromeOs::Uninstall(const std::string& app_id,
                                        gfx::NativeWindow parent_window) {
  UninstallImpl(app_id, parent_window, base::DoNothing());
}

void AppServiceProxyChromeOs::PauseApps(
    const std::map<std::string, PauseData>& pause_data) {
  if (!app_service_.is_connected()) {
    return;
  }

  for (auto& data : pause_data) {
    apps::mojom::AppType app_type = app_registry_cache_.GetAppType(data.first);
    if (app_type == apps::mojom::AppType::kUnknown) {
      continue;
    }

    app_registry_cache_.ForOneApp(
        data.first, [this](const apps::AppUpdate& update) {
          if (update.Paused() != apps::mojom::OptionalBool::kTrue) {
            pending_pause_requests_.MaybeAddApp(update.AppId());
          }
        });

    // The app pause dialog can't be loaded for unit tests.
    if (!data.second.should_show_pause_dialog || is_using_testing_profile_) {
      app_service_->PauseApp(app_type, data.first);
      continue;
    }

    app_registry_cache_.ForOneApp(
        data.first, [this, &data](const apps::AppUpdate& update) {
          LoadIconForDialog(
              update,
              base::BindOnce(&AppServiceProxyChromeOs::OnLoadIconForPauseDialog,
                             weak_ptr_factory_.GetWeakPtr(), update.AppType(),
                             update.AppId(), update.Name(), data.second));
        });
  }
}

void AppServiceProxyChromeOs::UnpauseApps(
    const std::set<std::string>& app_ids) {
  if (!app_service_.is_connected()) {
    return;
  }

  for (auto& app_id : app_ids) {
    apps::mojom::AppType app_type = app_registry_cache_.GetAppType(app_id);
    if (app_type == apps::mojom::AppType::kUnknown) {
      continue;
    }

    pending_pause_requests_.MaybeRemoveApp(app_id);
    app_service_->UnpauseApps(app_type, app_id);
  }
}

void AppServiceProxyChromeOs::SetResizeLocked(
    const std::string& app_id,
    apps::mojom::OptionalBool locked) {
  if (app_service_.is_connected()) {
    apps::mojom::AppType app_type = app_registry_cache_.GetAppType(app_id);
    app_service_->SetResizeLocked(app_type, app_id, locked);
  }
}

void AppServiceProxyChromeOs::SetArcIsRegistered() {
  if (arc_is_registered_) {
    return;
  }

  arc_is_registered_ = true;
  extension_apps_->ObserveArc();
  if (web_apps_) {
    web_apps_->ObserveArc();
  }
}

void AppServiceProxyChromeOs::FlushMojoCallsForTesting() {
  app_service_impl_->FlushMojoCallsForTesting();
  if (built_in_chrome_os_apps_) {
    built_in_chrome_os_apps_->FlushMojoCallsForTesting();
  }
  crostini_apps_->FlushMojoCallsForTesting();
  extension_apps_->FlushMojoCallsForTesting();
  if (plugin_vm_apps_)
    plugin_vm_apps_->FlushMojoCallsForTesting();
  if (lacros_apps_) {
    lacros_apps_->FlushMojoCallsForTesting();
  }
  if (web_apps_) {
    web_apps_->FlushMojoCallsForTesting();
  }
  if (borealis_apps_) {
    borealis_apps_->FlushMojoCallsForTesting();
  }
  receivers_.FlushForTesting();
}

void AppServiceProxyChromeOs::ReInitializeCrostiniForTesting(Profile* profile) {
  if (app_service_.is_connected()) {
    crostini_apps_->ReInitializeForTesting(app_service_, profile);
  }
}

void AppServiceProxyChromeOs::SetDialogCreatedCallbackForTesting(
    base::OnceClosure callback) {
  dialog_created_callback_ = std::move(callback);
}

void AppServiceProxyChromeOs::UninstallForTesting(
    const std::string& app_id,
    gfx::NativeWindow parent_window,
    base::OnceClosure callback) {
  UninstallImpl(app_id, parent_window, std::move(callback));
}

void AppServiceProxyChromeOs::Shutdown() {
  uninstall_dialogs_.clear();

  if (app_service_.is_connected()) {
    extension_apps_->Shutdown();
    if (web_apps_) {
      web_apps_->Shutdown();
    }
  }
  borealis_apps_.reset();
}

void AppServiceProxyChromeOs::UninstallImpl(const std::string& app_id,
                                            gfx::NativeWindow parent_window,
                                            base::OnceClosure callback) {
  if (!app_service_.is_connected()) {
    return;
  }

  app_registry_cache_.ForOneApp(app_id, [this, parent_window, &callback](
                                            const apps::AppUpdate& update) {
    apps::mojom::IconKeyPtr icon_key = update.IconKey();
    auto uninstall_dialog = std::make_unique<UninstallDialog>(
        profile_, update.AppType(), update.AppId(), update.Name(),
        std::move(icon_key), this, parent_window,
        base::BindOnce(&AppServiceProxyChromeOs::OnUninstallDialogClosed,
                       weak_ptr_factory_.GetWeakPtr(), update.AppType(),
                       update.AppId()));
    uninstall_dialog->SetDialogCreatedCallbackForTesting(std::move(callback));
    uninstall_dialogs_.emplace(std::move(uninstall_dialog));
  });
}

void AppServiceProxyChromeOs::OnUninstallDialogClosed(
    apps::mojom::AppType app_type,
    const std::string& app_id,
    bool uninstall,
    bool clear_site_data,
    bool report_abuse,
    UninstallDialog* uninstall_dialog) {
  if (uninstall) {
    app_registry_cache_.ForOneApp(app_id, RecordAppBounce);

    app_service_->Uninstall(app_type, app_id,
                            apps::mojom::UninstallSource::kUser,
                            clear_site_data, report_abuse);
  }

  DCHECK(uninstall_dialog);
  auto it = uninstall_dialogs_.find(uninstall_dialog);
  DCHECK(it != uninstall_dialogs_.end());
  uninstall_dialogs_.erase(it);
}

bool AppServiceProxyChromeOs::MaybeShowLaunchPreventionDialog(
    const apps::AppUpdate& update) {
  if (update.AppId() == extension_misc::kChromeAppId) {
    return false;
  }

  // Return true, and load the icon for the app block dialog when the app
  // is blocked by policy.
  if (update.Readiness() == apps::mojom::Readiness::kDisabledByPolicy) {
    LoadIconForDialog(
        update,
        base::BindOnce(&AppServiceProxyChromeOs::OnLoadIconForBlockDialog,
                       weak_ptr_factory_.GetWeakPtr(), update.Name()));
    return true;
  }

  // Return true, and load the icon for the app pause dialog when the app
  // is paused.
  if (update.Paused() == apps::mojom::OptionalBool::kTrue ||
      pending_pause_requests_.IsPaused(update.AppId())) {
    chromeos::app_time::AppTimeLimitInterface* app_limit =
        chromeos::app_time::AppTimeLimitInterface::Get(profile_);
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
        update,
        base::BindOnce(&AppServiceProxyChromeOs::OnLoadIconForPauseDialog,
                       weak_ptr_factory_.GetWeakPtr(), update.AppType(),
                       update.AppId(), update.Name(), pause_data));
    return true;
  }

  // The app is not prevented from launching and we didn't show any dialog.
  return false;
}

void AppServiceProxyChromeOs::LoadIconForDialog(
    const apps::AppUpdate& update,
    apps::mojom::Publisher::LoadIconCallback callback) {
  apps::mojom::IconKeyPtr icon_key = update.IconKey();
  constexpr bool kAllowPlaceholderIcon = false;
  constexpr int32_t kIconSize = 48;
  auto icon_type =
      (base::FeatureList::IsEnabled(features::kAppServiceAdaptiveIcon))
          ? apps::mojom::IconType::kStandard
          : apps::mojom::IconType::kUncompressed;

  // For browser tests, load the app icon, because there is no family link
  // logo for browser tests.
  //
  // For non_child profile, load the app icon, because the app is blocked by
  // admin.
  if (!dialog_created_callback_.is_null() || !profile_->IsChild()) {
    LoadIconFromIconKey(update.AppType(), update.AppId(), std::move(icon_key),
                        icon_type, kIconSize, kAllowPlaceholderIcon,
                        std::move(callback));
    return;
  }

  // Load the family link kite logo icon for the app pause dialog or the app
  // block dialog for the child profile.
  LoadIconFromResource(icon_type, kIconSize, IDR_SUPERVISED_USER_ICON,
                       kAllowPlaceholderIcon, IconEffects::kNone,
                       std::move(callback));
}

void AppServiceProxyChromeOs::OnLoadIconForBlockDialog(
    const std::string& app_name,
    apps::mojom::IconValuePtr icon_value) {
  auto icon_type =
      (base::FeatureList::IsEnabled(features::kAppServiceAdaptiveIcon))
          ? apps::mojom::IconType::kStandard
          : apps::mojom::IconType::kUncompressed;
  if (icon_value->icon_type != icon_type) {
    return;
  }

  AppServiceProxyChromeOs::CreateBlockDialog(app_name, icon_value->uncompressed,
                                             profile_);

  // For browser tests, call the dialog created callback to stop the run loop.
  if (!dialog_created_callback_.is_null()) {
    std::move(dialog_created_callback_).Run();
  }
}

void AppServiceProxyChromeOs::OnLoadIconForPauseDialog(
    apps::mojom::AppType app_type,
    const std::string& app_id,
    const std::string& app_name,
    const PauseData& pause_data,
    apps::mojom::IconValuePtr icon_value) {
  auto icon_type =
      (base::FeatureList::IsEnabled(features::kAppServiceAdaptiveIcon))
          ? apps::mojom::IconType::kStandard
          : apps::mojom::IconType::kUncompressed;
  if (icon_value->icon_type != icon_type) {
    OnPauseDialogClosed(app_type, app_id);
    return;
  }

  AppServiceProxyChromeOs::CreatePauseDialog(
      app_type, app_name, icon_value->uncompressed, pause_data,
      base::BindOnce(&AppServiceProxyChromeOs::OnPauseDialogClosed,
                     weak_ptr_factory_.GetWeakPtr(), app_type, app_id));

  // For browser tests, call the dialog created callback to stop the run loop.
  if (!dialog_created_callback_.is_null()) {
    std::move(dialog_created_callback_).Run();
  }
}

void AppServiceProxyChromeOs::OnPauseDialogClosed(apps::mojom::AppType app_type,
                                                  const std::string& app_id) {
  bool should_pause_app = pending_pause_requests_.IsPaused(app_id);
  if (!should_pause_app) {
    app_registry_cache_.ForOneApp(
        app_id, [&should_pause_app](const apps::AppUpdate& update) {
          if (update.Paused() == apps::mojom::OptionalBool::kTrue) {
            should_pause_app = true;
          }
        });
  }
  if (should_pause_app) {
    app_service_->PauseApp(app_type, app_id);
  }
}

void AppServiceProxyChromeOs::OnAppUpdate(const apps::AppUpdate& update) {
  if ((update.PausedChanged() &&
       update.Paused() == apps::mojom::OptionalBool::kTrue) ||
      (update.ReadinessChanged() &&
       update.Readiness() == apps::mojom::Readiness::kUninstalledByUser)) {
    pending_pause_requests_.MaybeRemoveApp(update.AppId());
  }

  AppServiceProxyBase::OnAppUpdate(update);
}

ScopedOmitBuiltInAppsForTesting::ScopedOmitBuiltInAppsForTesting()
    : previous_omit_built_in_apps_for_testing_(
          g_omit_built_in_apps_for_testing_) {
  g_omit_built_in_apps_for_testing_ = true;
}

ScopedOmitBuiltInAppsForTesting::~ScopedOmitBuiltInAppsForTesting() {
  g_omit_built_in_apps_for_testing_ = previous_omit_built_in_apps_for_testing_;
}

ScopedOmitPluginVmAppsForTesting::ScopedOmitPluginVmAppsForTesting()
    : previous_omit_plugin_vm_apps_for_testing_(
          g_omit_plugin_vm_apps_for_testing_) {
  g_omit_plugin_vm_apps_for_testing_ = true;
}

ScopedOmitPluginVmAppsForTesting::~ScopedOmitPluginVmAppsForTesting() {
  g_omit_plugin_vm_apps_for_testing_ =
      previous_omit_plugin_vm_apps_for_testing_;
}

}  // namespace apps
