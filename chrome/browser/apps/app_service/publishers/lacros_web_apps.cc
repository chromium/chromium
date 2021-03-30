// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/lacros_web_apps.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "chrome/browser/apps/app_service/app_icon_factory.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "extensions/common/constants.h"

namespace apps {

LacrosWebApps::LacrosWebApps(
    const mojo::Remote<apps::mojom::AppService>& app_service) {
  PublisherBase::Initialize(app_service, apps::mojom::AppType::kWeb);
}

LacrosWebApps::~LacrosWebApps() = default;

void LacrosWebApps::Connect(
    mojo::PendingRemote<apps::mojom::Subscriber> subscriber_remote,
    apps::mojom::ConnectOptionsPtr opts) {
  mojo::Remote<apps::mojom::Subscriber> subscriber(
      std::move(subscriber_remote));
  subscribers_.Add(std::move(subscriber));
}

void LacrosWebApps::LoadIcon(const std::string& app_id,
                             apps::mojom::IconKeyPtr icon_key,
                             apps::mojom::IconType icon_type,
                             int32_t size_hint_in_dip,
                             bool allow_placeholder_icon,
                             LoadIconCallback callback) {
  // TODO(crbug.com/1144877): Implement this.
  std::move(callback).Run(apps::mojom::IconValue::New());
}

void LacrosWebApps::Launch(const std::string& app_id,
                           int32_t event_flags,
                           apps::mojom::LaunchSource launch_source,
                           apps::mojom::WindowInfoPtr window_info) {
  // TODO(crbug.com/1144877): Implement this.
}

}  // namespace apps
