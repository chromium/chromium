// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/subscriber_crosapi.h"

#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "components/services/app_service/public/mojom/types.mojom.h"

namespace apps {

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

}  // namespace apps
