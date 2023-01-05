// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/arc_vm_data_migration_screen.h"

#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/arc_util.h"
#include "ash/components/arc/session/arc_vm_data_migration_status.h"
#include "base/notreached.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/prefs/pref_service.h"

namespace ash {

namespace {

constexpr char kUserActionSkip[] = "skip";
constexpr char kUserActionUpdate[] = "update";

}  // namespace

ArcVmDataMigrationScreen::ArcVmDataMigrationScreen(
    base::WeakPtr<ArcVmDataMigrationScreenView> view)
    : BaseScreen(ArcVmDataMigrationScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      view_(std::move(view)) {
  DCHECK(view_);
}

ArcVmDataMigrationScreen::~ArcVmDataMigrationScreen() = default;

void ArcVmDataMigrationScreen::ShowImpl() {
  // The migration screen is shown after a session restart with an ARC-enabled
  // login user, and thus the primary profile is available at this point.
  profile_ = ProfileManager::GetPrimaryUserProfile();
  DCHECK(profile_);
  SetUpInitialView();
  // TODO(b/258278176): Stop stale ARCVM and Upstart jobs before calling Show().
  if (view_) {
    view_->Show();
  }
}

void ArcVmDataMigrationScreen::HideImpl() {}

void ArcVmDataMigrationScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  VLOG(1) << "User action: action_id=" << action_id;
  if (action_id == kUserActionSkip) {
    HandleSkip();
  } else if (action_id == kUserActionUpdate) {
    HandleUpdate();
  } else {
    BaseScreen::OnUserAction(args);
  }
}

void ArcVmDataMigrationScreen::SetUpInitialView() {
  // TODO(b/258278176): Check free disk space and battery state.
  arc::ArcVmDataMigrationStatus data_migration_status =
      arc::GetArcVmDataMigrationStatus(profile_->GetPrefs());
  switch (data_migration_status) {
    case arc::ArcVmDataMigrationStatus::kConfirmed:
      // Set the status back to kNotified to prepare for cases where the
      // migration is skipped or the device is shut down before the migration is
      // started.
      arc::SetArcVmDataMigrationStatus(
          profile_->GetPrefs(), arc::ArcVmDataMigrationStatus::kNotified);
      UpdateUIState(ArcVmDataMigrationScreenView::UIState::kWelcome);
      break;
    case arc::ArcVmDataMigrationStatus::kStarted:
      // TODO(b/258278176): Show the resume screen.
      UpdateUIState(ArcVmDataMigrationScreenView::UIState::kWelcome);
      break;
    default:
      NOTREACHED();
      break;
  }
}

void ArcVmDataMigrationScreen::UpdateUIState(
    ArcVmDataMigrationScreenView::UIState state) {
  if (view_) {
    view_->SetUIState(state);
  }
}

void ArcVmDataMigrationScreen::HandleSkip() {
  chrome::AttemptRelaunch();
}

void ArcVmDataMigrationScreen::HandleUpdate() {
  arc::SetArcVmDataMigrationStatus(profile_->GetPrefs(),
                                   arc::ArcVmDataMigrationStatus::kStarted);
  // TODO(b/258278176): Trigger the migration.
  NOTIMPLEMENTED();
}

}  // namespace ash
