// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/subscriber_crosapi.h"

#include <utility>

#include "base/bind.h"
#include "base/notreached.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/intent_util.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/apps/app_service/metrics/app_service_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/webui/settings/ash/app_management/app_management_uma.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/features.h"
#include "components/services/app_service/public/mojom/types.mojom.h"

namespace {

bool Accepts(apps::AppType app_type) {
  return app_type == apps::AppType::kUnknown ||
         app_type == apps::AppType::kArc || app_type == apps::AppType::kWeb ||
         app_type == apps::AppType::kSystemWeb ||
         app_type == apps::AppType::kStandaloneBrowserChromeApp;
}

bool Accepts(const std::vector<apps::mojom::AppPtr>& deltas) {
  for (const auto& delta : deltas) {
    if (!Accepts(apps::ConvertMojomAppTypToAppType(delta->app_type))) {
      return false;
    }
  }
  return true;
}

}  // namespace

namespace apps {

class AppUpdate;

SubscriberCrosapi::SubscriberCrosapi(Profile* profile)
    : profile_(profile),
      proxy_(apps::AppServiceProxyFactory::GetForProfile(profile)) {}

SubscriberCrosapi::~SubscriberCrosapi() = default;

void SubscriberCrosapi::RegisterAppServiceProxyFromCrosapi(
    mojo::PendingReceiver<crosapi::mojom::AppServiceProxy> receiver) {
  // At the moment the app service subscriber will only accept one client
  // connect to ash chrome. Any extra clients will be ignored.
  // TODO(crbug.com/1174246): Support SxS lacros.
  if (crosapi_receiver_.is_bound()) {
    return;
  }
  crosapi_receiver_.Bind(std::move(receiver));
  crosapi_receiver_.set_disconnect_handler(base::BindOnce(
      &SubscriberCrosapi::OnCrosapiDisconnected, base::Unretained(this)));
}

void SubscriberCrosapi::OnApps(const std::vector<AppPtr>& deltas) {
  if (!subscriber_.is_bound()) {
    return;
  }

  std::vector<AppPtr> apps;
  for (const auto& delta : deltas) {
    if (Accepts(delta->app_type)) {
      apps.push_back(delta->Clone());
    }
  }

  // Apps are sent to Lacros side for preferred apps only, so we don't need to
  // set initialized status.
  subscriber_->OnApps(std::move(apps), AppType::kUnknown,
                      /*should_notify_initialized=*/false);
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

void SubscriberCrosapi::OnApps(std::vector<apps::mojom::AppPtr> deltas,
                               apps::mojom::AppType mojom_app_type,
                               bool should_notify_initialized) {
  // The non mojom OnApps is used to publish apps.
  return;
}

void SubscriberCrosapi::OnCapabilityAccesses(
    std::vector<apps::mojom::CapabilityAccessPtr> deltas) {
  NOTIMPLEMENTED();
}

void SubscriberCrosapi::Clone(
    mojo::PendingReceiver<apps::mojom::Subscriber> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void SubscriberCrosapi::OnPreferredAppsChanged(
    apps::mojom::PreferredAppChangesPtr changes) {
  if (!subscriber_.is_bound()) {
    return;
  }
  subscriber_->OnPreferredAppsChanged(
      ConvertMojomPreferredAppChangesToPreferredAppChanges(changes));
}

void SubscriberCrosapi::InitializePreferredApps(
    std::vector<apps::mojom::PreferredAppPtr> preferred_apps) {
  if (!subscriber_.is_bound()) {
    return;
  }
  subscriber_->InitializePreferredApps(
      ConvertMojomPreferredAppsToPreferredApps(preferred_apps));
}

void SubscriberCrosapi::OnCrosapiDisconnected() {
  crosapi_receiver_.reset();
  subscriber_.reset();
}

void SubscriberCrosapi::RegisterAppServiceSubscriber(
    mojo::PendingRemote<crosapi::mojom::AppServiceSubscriber> subscriber) {
  // At the moment the app service subscriber will only accept one client
  // connect to ash chrome. Any extra clients will be ignored.
  // TODO(crbug.com/1174246): Support SxS lacros.
  if (subscriber_.is_bound()) {
    return;
  }
  subscriber_.Bind(std::move(subscriber));
  subscriber_.set_disconnect_handler(base::BindOnce(
      &SubscriberCrosapi::OnSubscriberDisconnected, base::Unretained(this)));

  mojo::Remote<apps::mojom::AppService>& app_service = proxy_->AppService();
  DCHECK(app_service.is_bound());
  mojo::PendingRemote<apps::mojom::Subscriber> app_service_subscriber;
  receivers_.Add(this, app_service_subscriber.InitWithNewPipeAndPassReceiver());
  app_service->RegisterSubscriber(std::move(app_service_subscriber), nullptr);

  proxy_->RegisterCrosApiSubScriber(this);
}

void SubscriberCrosapi::Launch(crosapi::mojom::LaunchParamsPtr launch_params) {
  // TODO(crbug.com/1244506): Link up the return callback.
  proxy_->LaunchAppWithParams(
      ConvertCrosapiToLaunchParams(launch_params, profile_), base::DoNothing());
}

void SubscriberCrosapi::LoadIcon(const std::string& app_id,
                                 IconKeyPtr icon_key,
                                 IconType icon_type,
                                 int32_t size_hint_in_dip,
                                 apps::LoadIconCallback callback) {
  if (!icon_key) {
    std::move(callback).Run(std::make_unique<IconValue>());
    return;
  }

  proxy_->LoadIconFromIconKey(proxy_->AppRegistryCache().GetAppType(app_id),
                              app_id, *icon_key, icon_type, size_hint_in_dip,
                              /*allow_placeholder_icon=*/false,
                              std::move(callback));
}

void SubscriberCrosapi::AddPreferredApp(const std::string& app_id,
                                        crosapi::mojom::IntentPtr intent) {
  if (base::FeatureList::IsEnabled(kAppServicePreferredAppsWithoutMojom)) {
    proxy_->AddPreferredApp(
        app_id, apps_util::CreateAppServiceIntentFromCrosapi(intent, profile_));
  } else {
    proxy_->AddPreferredApp(
        app_id, apps_util::ConvertCrosapiToAppServiceIntent(intent, profile_));
  }
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
  proxy_->UninstallSilently(
      app_id, ConvertUninstallSourceToMojomUninstallSource(uninstall_source));
}

void SubscriberCrosapi::OnSubscriberDisconnected() {
  subscriber_.reset();
}

}  // namespace apps
