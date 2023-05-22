// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/video_conference_utils.h"

#include <string>

#include "ash/system/video_conference/effects/video_conference_tray_effects_manager_types.h"

namespace ash::video_conference_utils {

namespace {

std::string GetEffectHistogramNameBase(VcEffectId effect_id) {
  switch (effect_id) {
    case VcEffectId::kTestEffect:
      return "Ash.VideoConferenceTray.TestEffect";
    case VcEffectId::kBackgroundBlur:
      return "Ash.VideoConferenceTray.BackgroundBlur";
    case VcEffectId::kPortraitRelighting:
      return "Ash.VideoConferenceTray.PortraitRelighting";
    case VcEffectId::kNoiseCancellation:
      return "Ash.VideoConferenceTray.NoiseCancellation";
    case VcEffectId::kLiveCaption:
      return "Ash.VideoConferenceTray.LiveCaption";
    case VcEffectId::kCameraFraming:
      return "Ash.VideoConferenceTray.CameraFraming";
  }
}

}  // namespace

std::string GetEffectHistogramNameForClick(VcEffectId effect_id) {
  return GetEffectHistogramNameBase(effect_id) + ".Click";
}

std::string GetEffectHistogramNameForInitialState(VcEffectId effect_id) {
  return GetEffectHistogramNameBase(effect_id) + ".InitialState";
}

}  // namespace ash::video_conference_utils