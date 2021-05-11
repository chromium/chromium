// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/web_apps_crosapi.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "chrome/browser/apps/app_service/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "extensions/common/constants.h"

namespace {

std::vector<apps::mojom::AppPtr> CloneApps(
    const std::vector<apps::mojom::AppPtr>& clone_from) {
  std::vector<apps::mojom::AppPtr> clone_to;
  for (const auto& app : clone_from) {
    clone_to.push_back(app->Clone());
  }
  return clone_to;
}

}  // namespace

namespace apps {

WebAppsCrosapi::WebAppsCrosapi(Profile* profile) {
  // This object may be created when the flag is on or off, but only register
  // the publisher if the flag is on.
  if (base::FeatureList::IsEnabled(features::kWebAppsCrosapi)) {
    apps::AppServiceProxyChromeOs* proxy =
        apps::AppServiceProxyFactory::GetForProfile(profile);
    mojo::Remote<apps::mojom::AppService>& app_service = proxy->AppService();
    if (!app_service.is_bound()) {
      return;
    }
    PublisherBase::Initialize(app_service, apps::mojom::AppType::kWeb);
  }
}

WebAppsCrosapi::~WebAppsCrosapi() = default;

void WebAppsCrosapi::RegisterWebAppsCrosapiHost(
    mojo::PendingReceiver<crosapi::mojom::AppPublisher> receiver) {
  // At the moment the app service publisher will only accept one client
  // publishing apps to ash chrome. Any extra clients will be ignored.
  // TODO(crbug.com/1174246): Support SxS lacros.
  if (receiver_.is_bound()) {
    return;
  }
  receiver_.Bind(std::move(receiver));
  receiver_.set_disconnect_handler(base::BindOnce(
      &WebAppsCrosapi::OnCrosapiDisconnected, base::Unretained(this)));
}

void WebAppsCrosapi::Connect(
    mojo::PendingRemote<apps::mojom::Subscriber> subscriber_remote,
    apps::mojom::ConnectOptionsPtr opts) {
  mojo::Remote<apps::mojom::Subscriber> subscriber(
      std::move(subscriber_remote));
  subscribers_.Add(std::move(subscriber));
}

void WebAppsCrosapi::LoadIcon(const std::string& app_id,
                              apps::mojom::IconKeyPtr icon_key,
                              apps::mojom::IconType icon_type,
                              int32_t size_hint_in_dip,
                              bool allow_placeholder_icon,
                              LoadIconCallback callback) {
  // TODO(crbug.com/1144877): Implement this.
  std::move(callback).Run(apps::mojom::IconValue::New());
}

void WebAppsCrosapi::Launch(const std::string& app_id,
                            int32_t event_flags,
                            apps::mojom::LaunchSource launch_source,
                            apps::mojom::WindowInfoPtr window_info) {
  // TODO(crbug.com/1144877): Implement this.
}

void WebAppsCrosapi::OnApps(std::vector<apps::mojom::AppPtr> deltas) {
  if (!base::FeatureList::IsEnabled(features::kWebAppsCrosapi))
    return;
  for (auto& subscriber : subscribers_) {
    subscriber->OnApps(CloneApps(deltas), apps::mojom::AppType::kWeb,
                       false /* should_notify_initialized */);
  }
}

void WebAppsCrosapi::RegisterAppController(
    mojo::PendingRemote<crosapi::mojom::AppController> controller) {
  if (controller_.is_bound()) {
    return;
  }
  controller_.Bind(std::move(controller));
  controller_.set_disconnect_handler(base::BindOnce(
      &WebAppsCrosapi::OnControllerDisconnected, base::Unretained(this)));
}

void WebAppsCrosapi::OnCrosapiDisconnected() {
  receiver_.reset();
}

void WebAppsCrosapi::OnControllerDisconnected() {
  controller_.reset();
}
}  // namespace apps
