// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_USER_EDUCATION_HOLDING_SPACE_WALLPAPER_NUDGE_HOLDING_SPACE_WALLPAPER_NUDGE_CONTROLLER_H_
#define ASH_USER_EDUCATION_HOLDING_SPACE_WALLPAPER_NUDGE_HOLDING_SPACE_WALLPAPER_NUDGE_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/user_education/user_education_feature_controller.h"

namespace ash {

// Controller responsible for the Holding Space wallpaper nudge feature. Note
// that the `HoldingSpaceWallpaperNudgeController` is owned by the
// `UserEducationController` and exists if and only if the Holding Space
// wallpaper nudge feature is enabled.
class ASH_EXPORT HoldingSpaceWallpaperNudgeController
    : public UserEducationFeatureController {
 public:
  // Names for layers so they are easy to distinguish in debugging/testing.
  static constexpr char kHighlightLayerName[] =
      "HoldingSpaceWallpaperNudgeController::Highlight";

  HoldingSpaceWallpaperNudgeController();
  HoldingSpaceWallpaperNudgeController(
      const HoldingSpaceWallpaperNudgeController&) = delete;
  HoldingSpaceWallpaperNudgeController& operator=(
      const HoldingSpaceWallpaperNudgeController&) = delete;
  ~HoldingSpaceWallpaperNudgeController() override;

  // Returns the singleton instance owned by the `UserEducationController`.
  // NOTE: Exists if and only if the Holding Space wallpaper nudge feature is
  // enabled.
  static HoldingSpaceWallpaperNudgeController* Get();
};

}  // namespace ash

#endif  // ASH_USER_EDUCATION_HOLDING_SPACE_WALLPAPER_NUDGE_HOLDING_SPACE_WALLPAPER_NUDGE_CONTROLLER_H_
