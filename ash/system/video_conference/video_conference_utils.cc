// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/video_conference_utils.h"

#include <string>

#include "ash/system/video_conference/effects/video_conference_tray_effects_manager_types.h"

namespace ash::video_conference_utils {

std::string GetEffectHistogramName(VcEffectId effect_id) {
  switch (effect_id) {
    case VcEffectId::kTestEffect:
      return "Ash.VideoConferenceTray.TestEffect.Click";
    case VcEffectId::kBackgroundBlur:
      return "Ash.VideoConferenceTray.BackgroundBlur.Click";
    case VcEffectId::kPortraitRelighting:
      return "Ash.VideoConferenceTray.PortraitRelighting.Click";
    case VcEffectId::kNoiseCancellation:
      return "Ash.VideoConferenceTray.NoiseCancellation.Click";
    case VcEffectId::kLiveCaption:
      return "Ash.VideoConferenceTray.LiveCaption.Click";
    case VcEffectId::kCameraFraming:
      return "Ash.VideoConferenceTray.CameraFraming.Click";
  }
}

}  // namespace ash::video_conference_utils