// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_GRADUATION_GRADUATION_NUDGE_CONTROLLER_H_
#define ASH_SYSTEM_GRADUATION_GRADUATION_NUDGE_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"

class PrefService;

namespace ash {
struct ShelfID;
}

namespace ash::graduation {

// Class that handles the showing of the nudge for the Graduation app.
class ASH_EXPORT GraduationNudgeController {
 public:
  // The failure reason used to record a metric tracking failures to show the
  // nudge. Should be kept in sync with ContentTransferShowNudgeFailureReason
  // in tools/metrics/histograms/metadata/ash/enums.xml.
  enum class ShowNudgeFailureReason : int {
    // The app icon is unavailable.
    kAppIconUnavailable = 0,
    kMaxValue = kAppIconUnavailable,
  };

  // Should be kept consistent with the name in
  // tools/metrics/histograms/metadata/ash/histograms.xml.
  static constexpr char kShowNudgeFailedHistogramName[] =
      "Ash.ContentTransfer.ShowNudgeFailed";

  explicit GraduationNudgeController(PrefService* pref_service);
  GraduationNudgeController(const GraduationNudgeController&) = delete;
  GraduationNudgeController& operator=(const GraduationNudgeController&) =
      delete;
  ~GraduationNudgeController();

  // Shows the Graduation nudge for the item corresponding to the ShelfID that
  // is passed in. If the nudge has been shown already, the item is not visible
  // in the shelf, or if the shelf is not visible, then the nudge is not shown.
  void MaybeShowNudge(const ShelfID& id);

  // Sets kGraduationNudgeShown pref to false.
  void ResetNudgePref();

 private:
  raw_ptr<PrefService> pref_service_ = nullptr;
};
}  // namespace ash::graduation

#endif  // ASH_SYSTEM_GRADUATION_GRADUATION_NUDGE_CONTROLLER_H_
