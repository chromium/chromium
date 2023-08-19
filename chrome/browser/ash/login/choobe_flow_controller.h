// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_CHOOBE_FLOW_CONTROLLER_H_
#define CHROME_BROWSER_ASH_LOGIN_CHOOBE_FLOW_CONTROLLER_H_

#include <vector>

#include "base/containers/flat_set.h"
#include "base/values.h"
#include "chrome/browser/ash/login/oobe_screen.h"

class PrefService;

namespace ash {

// Controls the CHOOBE flow which is a part of the onboarding flow.
// CHOOBE Flow consists of a list of optional screens. The user can
// specify which optional screens to go through from the CHOOBE screen.
class ChoobeFlowController {
 public:
  ChoobeFlowController();

  ChoobeFlowController(const ChoobeFlowController&) = delete;
  ChoobeFlowController& operator=(const ChoobeFlowController&) = delete;

  ~ChoobeFlowController();

  // Whether CHOOBE flow should be started.
  // Precondition: Should only be called if CHOOBE was not started before.
  // To check whether CHOOBE has started and should be resumed use
  // ShouldResumeChoobe();
  static bool ShouldStartChoobe();

  // Whether CHOOBE flow was interrupted by a shutdown and should be resumed.
  // The check is based on whether `kChoobeSelectedScreens` or
  // `kChoobeCompletedScreens` prefs are stored.
  static bool ShouldResumeChoobe(const PrefService& prefs);

  // Whether the screen is one of the optional screens that can be shown in
  // CHOOBE.
  static bool IsOptionalScreen(OobeScreenId id);

  // Resume CHOOBE flow by loading stored prefs.
  // Precondition: `ShouldResumeChoobe()` returns true.
  void ResumeChoobe(const PrefService& prefs);

  // Returns `true` if CHOOBE has started and the user has selected the screen
  // from CHOOBE screen.
  bool ShouldScreenBeSkipped(OobeScreenId screen_id);

  // Should be called once CHOOBE flow is completed.
  void OnChoobeFlowExit();

  // Returns summaries of screens that the user is eligible to go through.
  // Used to fill CHOOBE screen tiles. The order of screens tiles should match
  // the order of the returned vector.
  std::vector<ScreenSummary> GetEligibleScreensSummaries();

  // Called once the user has selected the screens to go through to persist
  // their selection.
  // This allows us to resume CHOOBE in case of unexpected shutdown.
  // Precondition: `screen_ids` order should match the relative order of
  // `kOptionalScreens.`
  void OnScreensSelected(PrefService& prefs, base::Value::List screens_ids);

  // Called once an optional screen is completed, this will reflect later on the
  // screen's tile in CHOOBE screen.
  void OnScreenCompleted(PrefService& prefs, OobeScreenId screen_id);

  // Returns true if the screen is completed.
  bool IsScreenCompleted(OobeScreenId screen_id);

  // The return button in optional screens is only shown if the current screen
  // is the last selected screen, and there are still unselected screens.
  bool ShouldShowReturnButton(OobeScreenId screen_id);

 private:
  static bool IsScreenEligible(OobeScreenId id);
  void EnsureEligibleScreensPopulated();
  void ClearPreferences(PrefService& prefs);

  base::flat_set<OobeScreenId> eligible_screens_ids_;
  base::flat_set<OobeScreenId> selected_screens_ids_;
  base::flat_set<OobeScreenId> completed_screens_ids_;

  // CHOOBE Flow starts being active once `EnsureEligibleScreensPopulated()` is
  // called and returns to the inactive state when `OnChoobeFlowExit()` is
  // called.
  bool is_choobe_active_ = false;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_CHOOBE_FLOW_CONTROLLER_H_
