// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/subscriber_crosapi.h"

#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/types/optional_util.h"
#include "base/unguessable_token.h"
#include "chrome/browser/apps/app_service/app_install/app_install_service.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/intent_util.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/apps/app_service/metrics/app_service_metrics.h"
#include "chrome/browser/apps/browser_instance/browser_app_instance_registry.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/webui/ash/settings/app_management/app_management_uma.h"
#include "chromeos/crosapi/mojom/app_service_types.mojom.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "ui/gfx/native_widget_types.h"

namespace apps {

namespace {

bool Accepts(AppType app_type) {
  return app_type == AppType::kUnknown || app_type == AppType::kArc ||
         app_type == AppType::kWeb || app_type == AppType::kSystemWeb ||
         app_type == AppType::kStandaloneBrowserChromeApp;
}

std::optional<AppInstallSurface> AppInstallSurfaceFromCrosapi(
    crosapi::mojom::InstallAppParams::Surface surface) {
  using Surface = crosapi::mojom::InstallAppParams::Surface;
  switch (surface) {
    case Surface::kUnknown:
      return std::nullopt;
    case Surface::kAppInstallUriUnknown:
      return AppInstallSurface::kAppInstallUriUnknown;
    case Surface::kAppInstallUriShowoff:
      return AppInstallSurface::kAppInstallUriShowoff;
    case Surface::kAppInstallUriMall:
      return AppInstallSurface::kAppInstallUriMall;
    case Surface::kAppInstallUriGetit:
      return AppInstallSurface::kAppInstallUriGetit;
    case Surface::kAppInstallUriLauncher:
      return AppInstallSurface::kAppInstallUriLauncher;
    case Surface::kAppInstallUriPeripherals:
      return AppInstallSurface::kAppInstallUriPeripherals;
  }
}

}  // namespace

class AppUpdate;

SubscriberCrosapi::SubscriberCrosapi(Profile* profile)
    : profile_(profile),
      proxy_(apps::AppServiceProxyFactory::GetForProfile(profile)) {}

SubscriberCrosapi::~SubscriberCrosapi() = default;

void SubscriberCrosapi::RegisterAppServiceProxyFromCrosapi(
    mojo::PendingReceiver<crosapi::mojom::AppServiceProxy> receiver) {
  // At the moment the app service subscriber will only accept one client
  // connect to ash chrome. Any extra clients will be ignored.
  // TODO(crbug.com/40167449): Support SxS lacros.
  if (crosapi_receiver_.is_bound()) {
    return;
  }
  crosapi_receiver_.Bind(std::move(receiver));
  crosapi_receiver_.set_disconnect_handler(base::BindOnce(
      &SubscriberCrosapi::OnCrosapiDisconnected, base::Unretained(this)));
}

void SubscriberCrosapi::OnApps(const std::vector<AppPtr>& deltas,
                               AppType app_type,
                               bool should_notify_initialized) {
  if (!subscriber_.is_bound()) {
    return;
  }

  if (!Accepts(app_type)) {
    return;
  }

  std::vector<AppPtr> apps;
  for (const auto& delta : deltas) {
    if (Accepts(delta->app_type)) {
      apps.push_back(delta->Clone());
    }
  }

  subscriber_->OnApps(std::move(apps), app_type, should_notify_initialized);
}

void SubscriberCrosapi::InitializeApps() {
  // For each app type that has already been initialized, republish their apps
  // to Lacros as initialized. App types that are yet to initialize will
  // initialize via OnApps() in the usual way.

  // Sort apps by app type.
  std::vector<AppPtr> all_apps = proxy_->AppRegistryCache().GetAllApps();
  base::flat_map<AppType, std::vector<AppPtr>> app_type_apps;
  for (AppPtr& app : all_apps) {
    if (Accepts(app->app_type)) {
      // `update_version` holds an int value when get from AppRegistryCache.
      // However, at the Lacros side, AppServiceProxyLacros calls OnApps to
      // publish the app as `delta` to `app_registry_cache_` directly.
      // `icon_key`'s `update_version` should never hold a int32_t as `delta`
      // when calling OnApps to publish apps, and there is a bool checking for
      // `update_version` of `delta`'s `icon_key` in AppUpdate::Merge at the
      // Lacros side. So set `update_version` as false to prevent the crash from
      // the bool checking when merging to AppRegistryCache's `states_` at the
      // Lacros side .
      app->icon_key->update_version = false;
      app_type_apps[app->app_type].push_back(std::move(app));
    }
  }

  for (AppType app_type : proxy_->AppRegistryCache().InitializedAppTypes()) {
    if (Accepts(app_type)) {
      OnApps(std::move(app_type_apps[app_type]), app_type,
             /*should_notify_initialized=*/true);
    }
  }
}

void SubscriberCrosapi::InitializePreferredApps(PreferredApps preferred_apps) {
  if (subscriber_.is_bound()) {
    subscriber_->InitializePreferredApps(std::move(preferred_apps));
  }
}

void SubscriberCrosapi::OnPreferredAppsChanged(PreferredAppChangesPtr changes) {
  if (!subscriber_.is_bound()) {
    return;
  }
  subscriber_->OnPreferredAppsChanged(std::move(changes));
}

void SubscriberCrosapi::OnCrosapiDisconnected() {
  crosapi_receiver_.reset();
  subscriber_.reset();
}

void SubscriberCrosapi::RegisterAppServiceSubscriber(
    mojo::PendingRemote<crosapi::mojom::AppServiceSubscriber> subscriber) {
  // At the moment the app service subscriber will only accept one client
  // connect to ash chrome. Any extra clients will be ignored.
  // TODO(crbug.com/40167449): Support SxS lacros.
  if (subscriber_.is_bound()) {
    return;
  }
  subscriber_.Bind(std::move(subscriber));
  subscriber_.set_disconnect_handler(base::BindOnce(
      &SubscriberCrosapi::OnSubscriberDisconnected, base::Unretained(this)));

  proxy_->RegisterCrosApiSubScriber(this);
}

void SubscriberCrosapi::Launch(crosapi::mojom::LaunchParamsPtr launch_params) {
  // TODO(crbug.com/40787924): Link up the return callback.
  proxy_->LaunchAppWithParams(
      ConvertCrosapiToLaunchParams(launch_params, profile_), base::DoNothing());
}

void SubscriberCrosapi::LaunchWithResult(
    crosapi::mojom::LaunchParamsPtr launch_params,
    LaunchWithResultCallback callback) {
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile_);
  proxy->LaunchAppWithParams(
      ConvertCrosapiToLaunchParams(launch_params, profile_),
      MojomLaunchResultToLaunchResultCallback(std::move(callback)));
}

void SubscriberCrosapi::LoadIcon(const std::string& app_id,
                                 IconKeyPtr icon_key,
                                 IconType icon_type,
                                 int32_t size_hint_in_dip,
                                 apps::LoadIconCallback callback) {
  // Currently there is no usage of custom icon_key icon loading from
  // Lacros. Drop the icon key from the interface here.
  // TODO(crbug.com/40255408): Update the crosapi interface to match this.
  proxy_->LoadIcon(app_id, icon_type, size_hint_in_dip,
                   /*allow_placeholder_icon=*/false, std::move(callback));
}

void SubscriberCrosapi::AddPreferredAppDeprecated(
    const std::string& app_id,
    crosapi::mojom::IntentPtr intent) {
  NOTIMPLEMENTED();
}

void SubscriberCrosapi::ShowAppManagementPage(const std::string& app_id) {
  if (!proxy_->AppRegistryCache().ForOneApp(app_id,
                                            [](const apps::AppUpdate&) {})) {
    LOG(WARNING) << "Unknown app: " << app_id;
    return;
  }
  chrome::ShowAppManagementPage(
      profile_, app_id, ash::settings::AppManagementEntryPoint::kPageInfoView);
}

void SubscriberCrosapi::SetSupportedLinksPreference(const std::string& app_id) {
  proxy_->SetSupportedLinksPreference(app_id);
}

void SubscriberCrosapi::UninstallSilently(const std::string& app_id,
                                          UninstallSource uninstall_source) {
  proxy_->UninstallSilently(app_id, uninstall_source);
}

void SubscriberCrosapi::InstallAppWithFallback(
    crosapi::mojom::InstallAppParamsPtr params,
    InstallAppWithFallbackCallback callback) {
  bool valid = [&] {
    if (!params->serialized_package_id.has_value()) {
      return false;
    }

    std::optional<AppInstallSurface> surface =
        AppInstallSurfaceFromCrosapi(params->surface);
    if (!surface.has_value()) {
      return false;
    }

    std::optional<gfx::NativeWindow> window;
    if (params->window_id.has_value()) {
      window = proxy_->BrowserAppInstanceRegistry()->GetWindowByInstanceId(
          params->window_id.value());
    }

    proxy_->AppInstallService().InstallAppWithFallback(
        surface.value(), std::move(params->serialized_package_id).value(),
        window,
        base::BindOnce(std::move(callback),
                       crosapi::mojom::AppInstallResult::New()));
    return true;
  }();

  if (!valid) {
    std::move(callback).Run(crosapi::mojom::AppInstallResult::New());
  }
}

void SubscriberCrosapi::OnSubscriberDisconnected() {
  subscriber_.reset();
}

}  // namespace apps
