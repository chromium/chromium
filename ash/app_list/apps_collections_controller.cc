// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/apps_collections_controller.h"

#include <memory>
#include <optional>
#include <utility>

#include "ash/public/cpp/app_list/app_list_client.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/shell.h"
#include "base/metrics/histogram_functions.h"

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
  if (apps_collections_was_dissmissed_) {
    return false;
  }

  if (app_list_was_reordered_) {
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

  if (!session_controller->IsUserFirstLogin()) {
    return false;
  }

  // NOTE: Currently only supported for the primary user profile. This is a
  // self-imposed restriction.
  if (!session_controller->IsUserPrimary()) {
    return false;
  }

  // If the client was destroyed at this point, (i.e. in tests), return early to
  // avoid segmentation fault.
  if (!client_) {
    return false;
  }

  const std::optional<bool>& is_new_user =
      client_->IsNewUser(session_controller->GetActiveAccountId());

  // If it is not known whether the user is "new" or "existing" when this code
  // is reached, the user is treated as "existing" we want to err on the side of
  // being conservative.
  if (!is_new_user || !is_new_user.value()) {
    return false;
  }

  // To ensure the population number of the experiment groups (Counterfactual
  // and Enabled) are similar sized, query the finch experiment state here.
  // The counterfactual arm will serve as control group, so it should not show
  // Apps Collections even if it belong to the experiment.
  return app_list_features::IsAppsCollectionsEnabled() &&
         !app_list_features::IsAppsCollectionsEnabledCounterfactually();
}

void AppsCollectionsController::SetAppsCollectionDismissed(
    DismissReason reason) {
  apps_collections_was_dissmissed_ = true;

  if (reason == DismissReason::kSorting) {
    SetAppsReordered();
  }

  base::UmaHistogramEnumeration("Apps.AppList.AppsCollections.DismissedReason",
                                reason);
}

void AppsCollectionsController::SetAppsReordered() {
  app_list_was_reordered_ = true;
}

void AppsCollectionsController::SetClient(AppListClient* client) {
  client_ = client;
}

void AppsCollectionsController::RequestAppReorder(AppListSortOrder order) {
  CHECK(reorder_callback_);

  reorder_callback_.Run(order);
}

void AppsCollectionsController::SetReorderCallback(ReorderCallback callback) {
  CHECK(callback);

  reorder_callback_ = std::move(callback);
}
}  // namespace ash
