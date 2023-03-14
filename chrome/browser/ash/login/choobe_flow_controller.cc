// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/choobe_flow_controller.h"

#include <vector>

#include "base/containers/flat_set.h"
#include "base/notreached.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/login/theme_selection_screen_handler.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"

namespace ash {

namespace {

const int kMinScreensToShowChoobe = 3;
const int kMaxScreensToShowChoobe = 10;

// Optional screens to be shown in CHOOBE screen. The screen tiles are
// shown in the same order they are listed in this list.
const StaticOobeScreenId kOptionalScreens[] = {
    ThemeSelectionScreenView::kScreenId,
};

bool IsOptionalScreen(OobeScreenId screen_id) {
  for (const auto& screen : kOptionalScreens) {
    if (screen.AsId() == screen_id) {
      return true;
    }
  }
  return false;
}

}  // namespace

ChoobeFlowController::ChoobeFlowController() {}

ChoobeFlowController::~ChoobeFlowController() {
  ClearPreferences(*ProfileManager::GetActiveUserProfile()->GetPrefs());
}

void ChoobeFlowController::ClearPreferences(PrefService& prefs) {
  prefs.ClearPref(prefs::kChoobeCompletedScreens);
  prefs.ClearPref(prefs::kChoobeSelectedScreens);
}

bool ChoobeFlowController::ShouldStartChoobe() {
  // Count eligible screens and start CHOOBE if the count falls inside the
  // range [kMinScreensToShowChoobe, kMaxScreensToShowChoobe]
  int eligible_count = 0;
  for (auto screen_id : kOptionalScreens) {
    eligible_count += IsScreenEligible(screen_id.AsId());
  }
  return eligible_count >= kMinScreensToShowChoobe &&
         eligible_count <= kMaxScreensToShowChoobe;
}

bool ChoobeFlowController::ShouldResumeChoobe(const PrefService& prefs) {
  return prefs.HasPrefPath(prefs::kChoobeSelectedScreens);
}

// Resume CHOOBE requires loading `kChoobeSelectedScreens` and
// `kChoobeCompletedScreens` preferences.
void ChoobeFlowController::ResumeChoobe(const PrefService& prefs) {
  // Fill `selected_screens_ids_` with screens stored in
  // `kChoobeSelectedScreens` preference if it exists in `kOptionalScreens`.
  const auto& selected_screens_ids =
      prefs.GetList(prefs::kChoobeSelectedScreens);
  for (const auto& screen_id : selected_screens_ids) {
    const auto id = OobeScreenId(screen_id.GetString());
    if (IsOptionalScreen(id)) {
      selected_screens_ids_.insert(id);
    } else {
      LOG(WARNING) << "The selected screen " << screen_id.GetString()
                   << "was not found during the resuming of CHOOBE.";
    }
  }

  // Fill `completed_screens_ids_` with screens stored in
  // `kChoobeCompletedScreens` preference if it exists in `kOptionalScreens`.
  const auto& completed_screens_ids =
      prefs.GetList(prefs::kChoobeCompletedScreens);
  for (const auto& screen_id : completed_screens_ids) {
    const auto id = OobeScreenId(screen_id.GetString());
    if (IsOptionalScreen(id)) {
      completed_screens_ids_.insert(id);
    } else {
      LOG(WARNING) << "The completed screen " << screen_id.GetString()
                   << "was not found during the resuming of CHOOBE.";
    }
  }
}

std::vector<ScreenSummary> ChoobeFlowController::GetEligibleScreensSummaries() {
  EnsureEligibleScreensPopulated();
  std::vector<ScreenSummary> summaries;
  for (auto screen_id : eligible_screens_ids_) {
    ScreenSummary summary = LoginDisplayHost::default_host()
                                ->GetWizardController()
                                ->GetScreen(screen_id)
                                ->GetScreenSummary();

    // Mark the screen as completed if it exists in completed_screens_ids_ set.
    summary.is_completed =
        completed_screens_ids_.find(screen_id) != completed_screens_ids_.end();
    summaries.push_back(summary);
  }
  return summaries;
}

bool ChoobeFlowController::ShouldScreenBeSkipped(OobeScreenId screen_id) {
  return selected_screens_ids_.find(screen_id) == selected_screens_ids_.end();
}

void ChoobeFlowController::OnScreensSelected(PrefService& prefs,
                                             base::Value::List screens_ids) {
  if (screens_ids.empty()) {
    NOTREACHED() << "screen_ids list should not be empty";
  }

  selected_screens_ids_.clear();
  for (const auto& screen_id : screens_ids) {
    const auto id = OobeScreenId(screen_id.GetString());
    DCHECK(IsOptionalScreen(id));
    selected_screens_ids_.insert(id);
  }

  prefs.SetList(prefs::kChoobeSelectedScreens, std::move(screens_ids));
}

// Once a screen is completed, `completed_screens_ids_` should be updated so the
// screen tile is marked as completed in the next time the CHOOBE screen is
// shown. `completed_screens_ids_` should also be persisted to handle the
// unexpected shutdown and resuming the onboarding case.
void ChoobeFlowController::OnScreenCompleted(PrefService& prefs,
                                             OobeScreenId completed_screen_id) {
  if (!IsOptionalScreen(completed_screen_id)) {
    NOTREACHED()
        << "completed_screen_id does not exist in kOptionalScreens list";
  }

  completed_screens_ids_.insert(completed_screen_id);

  // Update `kChoobeCompletedScreens` pref.
  base::Value::List screens_ids;
  for (const auto& screen_id : completed_screens_ids_) {
    screens_ids.Append(screen_id.name);
  }
  prefs.SetList(prefs::kChoobeCompletedScreens, std::move(screens_ids));
}

bool ChoobeFlowController::ShouldShowReturnButton(OobeScreenId screen_id) {
  DCHECK(!selected_screens_ids_.empty());

  // Get the last screen in `kOptionalScreens` that exists in
  // `selected_screen_ids_`
  OobeScreenId last_selected_screen = OOBE_SCREEN_UNKNOWN;
  for (auto id : kOptionalScreens) {
    if (selected_screens_ids_.find(id.AsId()) != selected_screens_ids_.end()) {
      last_selected_screen = id.AsId();
    }
  }

  if (screen_id != last_selected_screen) {
    return false;
  }

  // To show the return button, all eligible screens must be completed
  // except for the current one which is still being completed.
  if (completed_screens_ids_.size() + 1 != eligible_screens_ids_.size()) {
    return false;
  }

  DCHECK(completed_screens_ids_.find(screen_id) ==
         completed_screens_ids_.end());
  DCHECK(eligible_screens_ids_.find(screen_id) != eligible_screens_ids_.end());

  return true;
}

void ChoobeFlowController::EnsureEligibleScreensPopulated() {
  if (!eligible_screens_ids_.empty()) {
    return;
  }
  for (auto screen_id : kOptionalScreens) {
    if (completed_screens_ids_.find(screen_id.AsId()) !=
            completed_screens_ids_.end() ||
        IsScreenEligible(screen_id.AsId())) {
      eligible_screens_ids_.insert(screen_id.AsId());
    }
  }
}

bool ChoobeFlowController::IsScreenEligible(OobeScreenId id) {
  LoginDisplayHost* host = LoginDisplayHost::default_host();
  BaseScreen* screen_obj = host->GetWizardController()->GetScreen(id);
  return !screen_obj->ShouldBeSkipped(*host->GetWizardContext());
}

}  // namespace ash
