// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/subscriber_crosapi.h"

#include <utility>

#include "base/bind.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/services/app_service/public/mojom/types.mojom.h"

namespace apps {

SubscriberCrosapi::SubscriberCrosapi(Profile* profile) {
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile);
  mojo::Remote<apps::mojom::AppService>& app_service = proxy->AppService();
  DCHECK(app_service.is_bound());
  mojo::PendingRemote<apps::mojom::Subscriber> subscriber;
  receivers_.Add(this, subscriber.InitWithNewPipeAndPassReceiver());
  app_service->RegisterSubscriber(std::move(subscriber), nullptr);
}

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

void SubscriberCrosapi::OnApps(std::vector<apps::mojom::AppPtr> deltas,
                               apps::mojom::AppType app_type,
                               bool should_notify_initialized) {
  NOTIMPLEMENTED();
}

void SubscriberCrosapi::OnCapabilityAccesses(
    std::vector<apps::mojom::CapabilityAccessPtr> deltas) {
  NOTIMPLEMENTED();
}

void SubscriberCrosapi::Clone(
    mojo::PendingReceiver<apps::mojom::Subscriber> receiver) {
  NOTIMPLEMENTED();
}

void SubscriberCrosapi::OnPreferredAppSet(
    const std::string& app_id,
    apps::mojom::IntentFilterPtr intent_filter) {
  NOTIMPLEMENTED();
}

void SubscriberCrosapi::OnPreferredAppRemoved(
    const std::string& app_id,
    apps::mojom::IntentFilterPtr intent_filter) {
  NOTIMPLEMENTED();
}

void SubscriberCrosapi::InitializePreferredApps(
    PreferredAppsList::PreferredApps preferred_apps) {
  NOTIMPLEMENTED();
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
}

void SubscriberCrosapi::OnSubscriberDisconnected() {
  subscriber_.reset();
}

}  // namespace apps
