// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_IN_APP_TO_HOME_NUDGE_CONTROLLER_H_
#define ASH_SHELF_IN_APP_TO_HOME_NUDGE_CONTROLLER_H_

#include "ash/ash_export.h"

namespace ash {

class ShelfWidget;

// Used by Chrome to notify shelf status changes and update in app to home
// gesture contextual nudge UI.
class ASH_EXPORT InAppToHomeNudgeController {
 public:
  explicit InAppToHomeNudgeController(ShelfWidget* shelf_widget);
  InAppToHomeNudgeController(const InAppToHomeNudgeController&) = delete;
  InAppToHomeNudgeController& operator=(const InAppToHomeNudgeController&) =
      delete;
  ~InAppToHomeNudgeController();

  // Sets whether the in app to home nudge can be shown for the current shelf
  // state. If the nudge is allowed, controller may show the nudge if required.
  // If the nudge is not allowed, the nudge will be hidden if currently visible.
  void SetNudgeAllowedForCurrentShelf(bool in_tablet_mode,
                                      bool in_app_shelf,
                                      bool shelf_controls_visible);

 private:
  // pointer to the shelf widget that owns the drag handle anchoring the nudge.
  ShelfWidget* const shelf_widget_;
};

}  // namespace ash

#endif  // ASH_SHELF_IN_APP_TO_HOME_NUDGE_CONTROLLER_H_
