// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/apps_collections_controller.h"

#include <optional>

#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/shell.h"

namespace ash {
namespace {

// The singleton instance owned by `AppListController`.
AppsCollectionsController* g_instance = nullptr;

}  // namespace

// AppsCollectionsController ----------------------------------------

AppsCollectionsController::AppsCollectionsController() {
  CHECK_EQ(g_instance, nullptr);
  g_instance = this;
}

AppsCollectionsController::~AppsCollectionsController() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
AppsCollectionsController* AppsCollectionsController::Get() {
  return g_instance;
}

bool AppsCollectionsController::ShouldShowAppsCollection() {
  if (!app_list_features::IsAppsCollectionsEnabled()) {
    return false;
  }

  if (apps_collections_was_dissmissed_) {
    return false;
  }

  if (app_list_features::IsForceShowAppsCollectionsEnabled()) {
    return true;
  }

  const auto* const session_controller = Shell::Get()->session_controller();
  if (const auto user_type = session_controller->GetUserType();
      user_type != user_manager::UserType::kRegular) {
    return false;
  }

  if (session_controller->IsActiveAccountManaged()) {
    return false;
  }

  // TODO(anasalazar): Consider adding check for UserEdicationApi for new users
  // cross-device, similar to how UserEducation features check for new users.

  return session_controller->IsUserFirstLogin();
}

void AppsCollectionsController::SetAppsCollectionDismissed() {
  apps_collections_was_dissmissed_ = true;
}

}  // namespace ash
