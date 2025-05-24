// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/apps_collections_controller.h"

#include <memory>
#include <optional>
#include <utility>

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/app_list/app_list_client.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/metrics/histogram_functions.h"
#include "components/prefs/pref_service.h"

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

AppsCollectionsController::ExperimentalArm
AppsCollectionsController::GetUserExperimentalArm() {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  if (!prefs ||
      prefs->FindPreference(prefs::kLauncherAppsCollectionsExperimentArm)
          ->IsDefaultValue()) {
    return ExperimentalArm::kDefaultValue;
  }

  return static_cast<ExperimentalArm>(
      prefs->GetInteger(prefs::kLauncherAppsCollectionsExperimentArm));
}

void AppsCollectionsController::MaybeRecordUserExperimentStatePref(
    ExperimentalArm arm) {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();

  if (!prefs) {
    return;
  }

  if (prefs->FindPreference(prefs::kLauncherAppsCollectionsExperimentArm)
          ->IsDefaultValue()) {
    prefs->SetInteger(prefs::kLauncherAppsCollectionsExperimentArm,
                      static_cast<int>(arm));
  }
}

std::string
AppsCollectionsController::GetUserExperimentalArmAsHistogramSuffix() {
  switch (GetUserExperimentalArm()) {
    case ash::AppsCollectionsController::ExperimentalArm::kDefaultValue:
    case ash::AppsCollectionsController::ExperimentalArm::kControl:
      return "";
    case ash::AppsCollectionsController::ExperimentalArm::kCounterfactual:
      return ".Counterfactual";
    case ash::AppsCollectionsController::ExperimentalArm::kEnabled:
      return ".Enabled";
    case ash::AppsCollectionsController::ExperimentalArm::kModifiedOrder:
      return ".ModifiedOrder";
  }

  NOTREACHED();
}

void AppsCollectionsController::CalculateExperimentalArm() {
  if (GetUserExperimentalArm() != ExperimentalArm::kDefaultValue) {
    return;
  }

  const auto* const session_controller = Shell::Get()->session_controller();
  if (const auto user_type = session_controller->GetUserType();
      user_type != user_manager::UserType::kRegular) {
    MaybeRecordUserExperimentStatePref(ExperimentalArm::kControl);
    return;
  }

  if (session_controller->IsActiveAccountManaged()) {
    MaybeRecordUserExperimentStatePref(ExperimentalArm::kControl);
    return;
  }

  if (!session_controller->IsUserFirstLogin()) {
    MaybeRecordUserExperimentStatePref(ExperimentalArm::kControl);
    return;
  }

  // NOTE: Currently only supported for the primary user profile. This is a
  // self-imposed restriction.
  if (!session_controller->IsUserPrimary()) {
    MaybeRecordUserExperimentStatePref(ExperimentalArm::kControl);
    return;
  }

  // If the client was destroyed at this point, (i.e. in tests), return early to
  // avoid segmentation fault.
  if (!client_) {
    MaybeRecordUserExperimentStatePref(ExperimentalArm::kControl);
    return;
  }

  const std::optional<bool>& is_new_user =
      client_->IsNewUser(session_controller->GetActiveAccountId());

  // If it is not known whether the user is "new" or "existing" when this code
  // is reached, the user is treated as "existing" we want to err on the side of
  // being conservative.
  if (!is_new_user || !is_new_user.value()) {
    MaybeRecordUserExperimentStatePref(ExperimentalArm::kControl);
    return;
  }

  if (client_->HasReordered()) {
    MaybeRecordUserExperimentStatePref(ExperimentalArm::kControl);
    return;
  }

  // To ensure the population number of the experiment groups (Counterfactual
  // and Enabled) are similar sized, query the finch experiment state here.
  // The counterfactual arm will serve as control group, so it should not show
  // Apps Collections even if it belong to the experiment.

  if (!app_list_features::IsAppsCollectionsEnabled()) {
    MaybeRecordUserExperimentStatePref(ExperimentalArm::kControl);
    return;
  }

  if (app_list_features::IsAppsCollectionsEnabledCounterfactually()) {
    MaybeRecordUserExperimentStatePref(ExperimentalArm::kCounterfactual);
    return;
  }

  if (app_list_features::IsAppsCollectionsEnabledWithModifiedOrder()) {
    MaybeRecordUserExperimentStatePref(ExperimentalArm::kModifiedOrder);
    return;
  }
  MaybeRecordUserExperimentStatePref(ExperimentalArm::kEnabled);
  return;
}

bool AppsCollectionsController::ShouldShowAppsCollection() {
  if (apps_collections_was_dissmissed_) {
    return false;
  }

  if (app_list_was_reordered_) {
    return false;
  }

  if (force_apps_collections_) {
    return true;
  }

  if (GetUserExperimentalArm() == ExperimentalArm::kDefaultValue) {
    CalculateExperimentalArm();
  }

  return GetUserExperimentalArm() == ExperimentalArm::kEnabled &&
         Shell::Get()->session_controller()->IsUserFirstLogin();
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

void AppsCollectionsController::ForceAppsCollectionsForTesting(bool force) {
  force_apps_collections_ = force;
}
}  // namespace ash
