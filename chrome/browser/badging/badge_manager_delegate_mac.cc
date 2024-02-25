// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/badging/badge_manager_delegate_mac.h"

#include "chrome/browser/apps/app_shim/app_shim_manager_mac.h"
#include "chrome/browser/badging/badge_manager.h"

namespace badging {

BadgeManagerDelegateMac::BadgeManagerDelegateMac(Profile* profile,
                                                 BadgeManager* badge_manager)
    : BadgeManagerDelegate(profile, badge_manager) {}

void BadgeManagerDelegateMac::OnAppBadgeUpdated(const webapps::AppId& app_id) {
  const std::optional<BadgeManager::BadgeValue>& badge =
      badge_manager()->GetBadgeValue(app_id);

  auto* shim_manager = apps::AppShimManager::Get();
  if (!shim_manager) {
    return;
  }

  shim_manager->UpdateAppBadge(profile(), app_id, badge);
}

}  // namespace badging
