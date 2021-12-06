// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/phonehub/recent_apps_interaction_handler.h"
#include "ash/components/phonehub/notification.h"

namespace ash {
namespace phonehub {

RecentAppsInteractionHandler::RecentAppsInteractionHandler() = default;

RecentAppsInteractionHandler::~RecentAppsInteractionHandler() = default;

void RecentAppsInteractionHandler::AddRecentAppClickObserver(
    RecentAppClickObserver* observer) {
  observer_list_.AddObserver(observer);
}

void RecentAppsInteractionHandler::RemoveRecentAppClickObserver(
    RecentAppClickObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

}  // namespace phonehub
}  // namespace ash
