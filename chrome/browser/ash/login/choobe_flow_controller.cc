// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/choobe_flow_controller.h"

#include <vector>

#include "base/containers/flat_set.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/display_size_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/drive_pinning_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/theme_selection_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/touchpad_scroll_screen_handler.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"

namespace ash {

namespace {

const int kMinScreensToShowChoobe = 3;
const int kMaxScreensToShowChoobe = 10;

// Optional screens to be shown in CHOOBE screen. The screen tiles are
// shown in the same order they are listed in this list.
const StaticOobeScreenId kOptionalScreens[] = {
    TouchpadScrollScreenView::kScreenId,
    DrivePinningScreenView::kScreenId,
    DisplaySizeScreenView::kScreenId,
    ThemeSelectionScreenView::kScreenId,
};

// Returns the last screen in a set, given the order of screens in the
// `kOptionalScreens` array, if the set is empty, the function will return
// `OOBE_SCREEN_UNKNOWN`.
OobeScreenId GetLastOptionalScreenInSet(
    const base::flat_set<OobeScreenId>& screens) {
  OobeScreenId last_screen = OOBE_SCREEN_UNKNOWN;
  for (const auto& screen : kOptionalScreens) {
    if (screens.find(screen.AsId()) != screens.end()) {
      last_screen = screen.AsId();
    }
  }
  return last_screen;
}

}  // namespace

ChoobeFlowController::ChoobeFlowController() {}

ChoobeFlowController::~ChoobeFlowController() {}

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
  return prefs.HasPrefPath(prefs::kChoobeSelectedScreens) ||
         prefs.HasPrefPath(prefs::kChoobeCompletedScreens);
}

// Resume CHOOBE requires loading `kChoobeSelectedScreens` and
// `kChoobeCompletedScreens` preferences.
void ChoobeFlowController::ResumeChoobe(const PrefService& prefs) {
  EnsureEligibleScreensPopulated();

  // Fill `selected_screens_ids_` with screens stored in the
  // `kChoobeSelectedScreens` preference. It is necessary to recheck whether the
  // stored screen exists in `eligible_screens_ids_` because the screen could
  // have been made ineligible between the storing and the resuming of CHOOBE.
  const auto& selected_screens_ids =
      prefs.GetList(prefs::kChoobeSelectedScreens);
  for (const auto& screen_id : selected_screens_ids) {
    const auto id = OobeScreenId(screen_id.GetString());
    if (eligible_screens_ids_.find(id) != eligible_screens_ids_.end()) {
      selected_screens_ids_.insert(id);
    } else {
      LOG(WARNING) << "The selected screen " << screen_id.GetString()
                   << "is not eligible during the resuming of CHOOBE.";
    }
  }

  // Fill `completed_screens_ids_` with screens stored in the
  // `kChoobeCompletedScreens` preference. It is necessary to recheck whether
  // the stored screen exists in `eligible_screens_ids_` because the screen
  // could have been made ineligible between the storing and the resuming of
  // CHOOBE.
  const auto& completed_screens_ids =
      prefs.GetList(prefs::kChoobeCompletedScreens);
  for (const auto& screen_id : completed_screens_ids) {
    const auto id = OobeScreenId(screen_id.GetString());
    if (eligible_screens_ids_.find(id) != eligible_screens_ids_.end()) {
      completed_screens_ids_.insert(id);
    } else {
      LOG(WARNING) << "The completed screen " << screen_id.GetString()
                   << "is not eligible during the resuming of CHOOBE.";
    }
  }
}

std::vector<ScreenSummary> ChoobeFlowController::GetEligibleScreensSummaries() {
  EnsureEligibleScreensPopulated();
  std::vector<ScreenSummary> summaries;
  for (auto id : kOptionalScreens) {
    const auto screen_id = id.AsId();
    if (eligible_screens_ids_.find(screen_id) == eligible_screens_ids_.end()) {
      continue;
    }

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
  // If the CHOOBE Flow Controller has not stared yet, then the
  // controller will not object to showing the screen.
  if (!is_choobe_active_) {
    return false;
  }

  return selected_screens_ids_.find(screen_id) == selected_screens_ids_.end();
}

void ChoobeFlowController::OnScreensSelected(PrefService& prefs,
                                             base::Value::List screens_ids) {
  if (screens_ids.empty()) {
    NOTREACHED_IN_MIGRATION() << "screen_ids list should not be empty";
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
// If the last selected screen is completed, clear the selected screens.
void ChoobeFlowController::OnScreenCompleted(PrefService& prefs,
                                             OobeScreenId completed_screen_id) {
  if (!IsOptionalScreen(completed_screen_id)) {
    NOTREACHED_IN_MIGRATION()
        << "completed_screen_id does not exist in kOptionalScreens list";
  }

  completed_screens_ids_.insert(completed_screen_id);

  // Update `kChoobeCompletedScreens` pref.
  base::Value::List screens_ids;
  for (const auto& screen_id : completed_screens_ids_) {
    screens_ids.Append(screen_id.name);
  }
  prefs.SetList(prefs::kChoobeCompletedScreens, std::move(screens_ids));

  // If the last optional screen is completed, clear the selected screens.
  if (completed_screen_id ==
      GetLastOptionalScreenInSet(selected_screens_ids_)) {
    selected_screens_ids_.clear();
    prefs.ClearPref(prefs::kChoobeSelectedScreens);
  }
}

bool ChoobeFlowController::IsScreenCompleted(OobeScreenId id) {
  return completed_screens_ids_.find(id) != completed_screens_ids_.end();
}

void ChoobeFlowController::OnChoobeFlowExit() {
  base::UmaHistogramBoolean("OOBE.CHOOBE.FlowSkipped",
                            completed_screens_ids_.empty());

  for (auto static_id : kOptionalScreens) {
    OobeScreenId id = static_id.AsId();
    std::string screen_name = id.name;
    screen_name[0] = base::ToUpperASCII(screen_name[0]);
    std::string histogram_name = "OOBE.CHOOBE.ScreenCompleted." + screen_name;
    bool is_completed =
        completed_screens_ids_.find(id) != completed_screens_ids_.end();
    base::UmaHistogramBoolean(histogram_name, is_completed);
  }

  ClearPreferences(*ProfileManager::GetActiveUserProfile()->GetPrefs());
  is_choobe_active_ = false;
}

bool ChoobeFlowController::ShouldShowReturnButton(OobeScreenId screen_id) {
  DCHECK(!selected_screens_ids_.empty());

  // The return button should only be shown in the last selected screen.
  OobeScreenId last_selected_screen =
      GetLastOptionalScreenInSet(selected_screens_ids_);
  if (screen_id != last_selected_screen) {
    return false;
  }

  // To hide the return button, all eligible screens must be completed
  // except for the current one which is still being completed.
  if (completed_screens_ids_.size() + !IsScreenCompleted(screen_id) ==
      eligible_screens_ids_.size()) {
    return false;
  }

  DCHECK(eligible_screens_ids_.find(screen_id) != eligible_screens_ids_.end());

  return true;
}

void ChoobeFlowController::EnsureEligibleScreensPopulated() {
  if (is_choobe_active_) {
    return;
  }

  for (auto screen_id : kOptionalScreens) {
    if (IsScreenEligible(screen_id.AsId())) {
      eligible_screens_ids_.insert(screen_id.AsId());
    }
  }
  is_choobe_active_ = true;
}

bool ChoobeFlowController::IsScreenEligible(OobeScreenId id) {
  auto* host = LoginDisplayHost::default_host();
  auto* wizard_controller = host->GetWizardController();
  if (!wizard_controller->HasScreen(id)) {
    LOG(WARNING) << "Screen ID " << id
                 << " does not exist in wizard controller.";
    return false;
  }
  return !wizard_controller->GetScreen(id)->ShouldBeSkipped(
      *host->GetWizardContext());
}

bool ChoobeFlowController::IsOptionalScreen(OobeScreenId screen_id) {
  for (const auto& screen : kOptionalScreens) {
    if (screen.AsId() == screen_id) {
      return true;
    }
  }
  return false;
}

}  // namespace ash
