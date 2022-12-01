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
  // Resources of the strings used in the tiles shown in the CHOOBE
  // screen. Resources will be added to the `LocalizedValuesBuilder`
  // in `LocaleSwitchScreenHandler::DeclareLocalizedValues()`.
  struct OptionalScreenResource {
    const char* key;
    int message_id;
  };

  // Optional screen which is part of CHOOBE. The screen tile will
  // be shown in the CHOOBE screen if it is eligible for the user
  // (`Screen::ShouldBeSkipped()` method returns `false`).
  struct OptionalScreen {
    StaticOobeScreenId screen_id;
    const char* icon_id;
    OptionalScreenResource title_resource;
  };

  ChoobeFlowController();

  ChoobeFlowController(const ChoobeFlowController&) = delete;
  ChoobeFlowController& operator=(const ChoobeFlowController&) = delete;

  ~ChoobeFlowController();

  // Called before the CHOOBE screen is shown:
  //  * Populates the `eligible_screens_` vector with the optional
  //    screens that the user can go through.
  //  * Sets `is_choobe_flow_active_` to `true` if the number of eligible
  //    screens falls in the allowed range for CHOOBE screen to be shown.
  void Start();

  // * Clears `eligible_screens_` and `selected_screens_`.
  // * Sets `is_choobe_flow_active_` to `false` so that future calls to
  //   `ShouldScreenBeSkipped(screen_id)` returns `false`.
  // * Clears `kChoobeSelectedScreens` preference from `prefs`.
  void Stop(PrefService& prefs);

  // Returns `true` if CHOOBE is active and the user has selected the screen.
  bool ShouldScreenBeSkipped(OobeScreenId screen_id);

  // Returns screens that the user is eligible to go through.
  std::vector<OptionalScreen> GetEligibleCHOOBEScreens();

  // Returns whether a screen is one of CHOOBE oprional screens.
  static bool IsOptionalScreen(OobeScreenId screen_id);

  // Returns string resources for all optional screens stored in
  // `kNonFoundationalScreens`.
  static std::vector<OptionalScreenResource> GetOptionalScreensResources();

  // Populates `selected_screens_` with `screens_ids`.
  // Persists `screens_ids` using `prefs`.
  void OnScreensSelected(PrefService& prefs, base::Value::List screens_ids);

  bool IsChoobeFlowActive() { return is_choobe_flow_active_; }

  // If there are persisted selected screens list in `prefs`, insert its
  // items to `selected_screens_` set, and set `is_choobe_flow_active_`
  // to `true`.
  void MaybeResumeChoobe(const PrefService& prefs);

 private:
  // Screens that the user can select in the CHOOBE screen. Populated by
  // the `Start()` method.
  std::vector<OptionalScreen> eligible_screens_;

  // Screens that the user has selected. Populated by the `OnScreensSelected`
  // method.
  base::flat_set<OobeScreenId> selected_screens_;

  bool is_choobe_flow_active_ = false;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_CHOOBE_FLOW_CONTROLLER_H_
