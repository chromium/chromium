// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_BIRCH_BIRCH_PRIVACY_NUDGE_CONTROLLER_H_
#define ASH_WM_OVERVIEW_BIRCH_BIRCH_PRIVACY_NUDGE_CONTROLLER_H_

#include "ash/ash_export.h"

class PrefRegistrySimple;

namespace views {
class View;
}

namespace ash {

// Controller for showing the privacy nudge for Birch suggestion chips. It
// encourages users to open the context menu to control suggestion data types.
class ASH_EXPORT BirchPrivacyNudgeController {
 public:
  BirchPrivacyNudgeController();
  BirchPrivacyNudgeController(const BirchPrivacyNudgeController&) = delete;
  BirchPrivacyNudgeController& operator=(const BirchPrivacyNudgeController&) =
      delete;
  ~BirchPrivacyNudgeController();

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Records that the context menu was shown (and hence the nudge doesn't need
  // to be shown any more).
  static void DidShowContextMenu();

  // Attempts to show the nudge. The nudge will show if it hasn't been shown in
  // the past 24 hours, or if it has been shown less than three times. The
  // nudge will be anchored to `anchor_view` if provided.
  void MaybeShowNudge(views::View* anchor_view);
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_BIRCH_BIRCH_PRIVACY_NUDGE_CONTROLLER_H_
