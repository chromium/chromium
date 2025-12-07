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
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash::graduation {

namespace {
constexpr int kMaxNudgeShownCount = 3;
constexpr base::TimeDelta kNudgeTimeBetweenShown = base::Hours(24);
}  // namespace

GraduationNudgeController::GraduationNudgeController(
    PrefService* pref_service) {
  CHECK(pref_service);
  pref_service_ = pref_service;

  // TODO(b:374164026): Clean up this deprecated pref.
  pref_service_->ClearPref(prefs::kGraduationNudgeShownDeprecated);
}

GraduationNudgeController::~GraduationNudgeController() = default;

void GraduationNudgeController::MaybeShowNudge(const ShelfID& id) {
  int nudge_shown_count =
      pref_service_->GetInteger(prefs::kGraduationNudgeShownCount);
  if (nudge_shown_count >= kMaxNudgeShownCount) {
    return;
  }
  if (base::Time::Now() -
          pref_service_->GetTime(prefs::kGraduationNudgeLastShownTime) <
      kNudgeTimeBetweenShown) {
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
    VLOG(1) << "graduation: Tried to show nudge but app button not available";
    base::UmaHistogramEnumeration(kShowNudgeFailedHistogramName,
                                  ShowNudgeFailureReason::kAppIconUnavailable);
    return;
  }

  AnchoredNudgeData nudge_data(
      "graduation.nudge", NudgeCatalogName::kGraduationAppEnabled,
      l10n_util::GetStringUTF16(IDS_ASH_GRADUATION_NUDGE_TEXT), button);
  nudge_data.anchored_to_shelf = true;

  // Shows the nudge and records a metric tracking the number of times the nudge
  // has been shown to a user.
  Shell::Get()->anchored_nudge_manager()->Show(nudge_data);

  pref_service_->SetInteger(prefs::kGraduationNudgeShownCount,
                            ++nudge_shown_count);
  pref_service_->SetTime(prefs::kGraduationNudgeLastShownTime,
                         base::Time::Now());
}

void GraduationNudgeController::ResetNudgePref() {
  int nudge_shown =
      pref_service_->GetInteger(prefs::kGraduationNudgeShownCount);
  if (nudge_shown == 0) {
    VLOG(1) << "graduation: Nudge has not been shown but pref is being reset";
  }
  pref_service_->ClearPref(prefs::kGraduationNudgeShownCount);
  pref_service_->ClearPref(prefs::kGraduationNudgeLastShownTime);
}

}  // namespace ash::graduation
