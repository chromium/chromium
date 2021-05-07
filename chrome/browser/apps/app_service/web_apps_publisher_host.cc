// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/web_apps_publisher_host.h"

#include "base/bind.h"
#include "base/one_shot_event.h"
#include "chrome/browser/apps/app_service/web_apps_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chromeos/lacros/lacros_service.h"

namespace apps {

// static
crosapi::mojom::AppPublisher* WebAppsPublisherHost::publisher_for_testing_ =
    nullptr;

WebAppsPublisherHost::WebAppsPublisherHost(Profile* profile)
    : profile_(profile), provider_(web_app::WebAppProvider::Get(profile)) {
  // Allow for web app migration tests.
  if (!provider_->registrar().AsWebAppRegistrar())
    return;

  provider_->on_registry_ready().Post(
      FROM_HERE, base::BindOnce(&WebAppsPublisherHost::OnReady,
                                weak_ptr_factory_.GetWeakPtr()));
  registrar_observation_.Observe(&registrar());
}

WebAppsPublisherHost::~WebAppsPublisherHost() = default;

web_app::WebAppRegistrar& WebAppsPublisherHost::registrar() const {
  return *provider_->registrar().AsWebAppRegistrar();
}

crosapi::mojom::AppPublisher* WebAppsPublisherHost::GetPublisher() const {
  if (publisher_for_testing_) {
    return publisher_for_testing_;
  }
  auto* service = chromeos::LacrosService::Get();
  if (!service) {
    return nullptr;
  }
  if (!service->IsAvailable<crosapi::mojom::AppPublisher>()) {
    return nullptr;
  }
  if (!service->init_params()->web_apps_enabled) {
    return nullptr;
  }
  return service->GetRemote<crosapi::mojom::AppPublisher>().get();
}

// static
void WebAppsPublisherHost::SetPublisherForTesting(
    crosapi::mojom::AppPublisher* publisher) {
  publisher_for_testing_ = publisher;
}

void WebAppsPublisherHost::OnReady() {
  crosapi::mojom::AppPublisher* const publisher = GetPublisher();
  if (!publisher || !registrar_observation_.IsObserving()) {
    return;
  }

  std::vector<apps::mojom::AppPtr> apps;
  for (const web_app::WebApp& web_app : registrar().GetApps()) {
    apps.push_back(Convert(&web_app, apps::mojom::Readiness::kReady));
  }
  publisher->OnApps(std::move(apps));
}

// web_app::AppRegistrarObserver:
void WebAppsPublisherHost::OnWebAppInstalled(const web_app::AppId& app_id) {
  const web_app::WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    return;
  }

  Publish(Convert(web_app, apps::mojom::Readiness::kReady));
}

void WebAppsPublisherHost::OnWebAppWillBeUninstalled(
    const web_app::AppId& app_id) {
  const web_app::WebApp* web_app = GetWebApp(app_id);
  if (!web_app) {
    return;
  }

  Publish(
      apps_util::ConvertUninstalledWebApp(web_app, apps::mojom::AppType::kWeb));
}

void WebAppsPublisherHost::OnAppRegistrarDestroyed() {
  registrar_observation_.Reset();
}

const web_app::WebApp* WebAppsPublisherHost::GetWebApp(
    const web_app::AppId& app_id) const {
  return registrar().GetAppById(app_id);
}

apps::mojom::AppPtr WebAppsPublisherHost::Convert(
    const web_app::WebApp* web_app,
    apps::mojom::Readiness readiness) const {
  return apps_util::ConvertWebApp(profile_, web_app, apps::mojom::AppType::kWeb,
                                  readiness);
}

void WebAppsPublisherHost::Publish(apps::mojom::AppPtr app) {
  crosapi::mojom::AppPublisher* const publisher = GetPublisher();
  if (!publisher) {
    return;
  }

  std::vector<apps::mojom::AppPtr> apps;
  apps.push_back(std::move(app));
  publisher->OnApps(std::move(apps));
}

}  // namespace apps
