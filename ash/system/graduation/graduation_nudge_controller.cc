// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/graduation/graduation_nudge_controller.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/system/anchored_nudge_data.h"
#include "ash/shelf/hotseat_widget.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_app_button.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/toast/anchored_nudge_manager_impl.h"
#include "base/check.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash::graduation {

GraduationNudgeController::GraduationNudgeController(
    PrefService* pref_service) {
  CHECK(pref_service);
  pref_service_ = pref_service;
}

GraduationNudgeController::~GraduationNudgeController() = default;

void GraduationNudgeController::MaybeShowNudge(const ShelfID& id) {
  bool nudge_shown = pref_service_->GetBoolean(prefs::kGraduationNudgeShown);
  if (nudge_shown) {
    return;
  }

  Shelf* shelf = Shelf::ForWindow(Shell::GetPrimaryRootWindow());
  CHECK(shelf);
  HotseatWidget* hotseat_widget = shelf->hotseat_widget();
  if (hotseat_widget->state() == HotseatState::kHidden) {
    return;
  }

  ShelfAppButton* button =
      hotseat_widget->GetShelfView()->GetShelfAppButton(id);
  if (!button) {
    // TODO(b:365835134): Record metrics for failure to show the nudge.
    VLOG(1) << "graduation: Tried to show nudge but app button not available";
    return;
  }

  AnchoredNudgeData nudge_data(
      "graduation.nudge", NudgeCatalogName::kGraduationAppEnabled,
      l10n_util::GetStringUTF16(IDS_ASH_GRADUATION_NUDGE_TEXT), button);
  nudge_data.anchored_to_shelf = true;
  Shell::Get()->anchored_nudge_manager()->Show(nudge_data);
  pref_service_->SetBoolean(prefs::kGraduationNudgeShown, true);
}

void GraduationNudgeController::ResetNudgePref() {
  bool nudge_shown = pref_service_->GetBoolean(prefs::kGraduationNudgeShown);
  if (!nudge_shown) {
    VLOG(1) << "graduation: Nudge has not been shown but pref is being reset";
  }
  pref_service_->SetBoolean(prefs::kGraduationNudgeShown, false);
}

}  // namespace ash::graduation
