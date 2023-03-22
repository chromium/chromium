// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/video_conference_utils.h"

#include <string>

#include "ash/system/video_conference/effects/video_conference_tray_effects_manager_types.h"
#include "base/strings/string_util.h"

namespace ash::video_conference_utils {

namespace {

constexpr char kTestEffectHistogramName[] = "TestEffect";
constexpr char kBackgroundBlurHistogramName[] = "BackgroundBlur";
constexpr char kPortraitRelightingHistogramName[] = "PortraitRelighting";
constexpr char kNoiseCancellationHistogramName[] = "NoiseCancellation";
constexpr char kLiveCaptionHistogramName[] = "LiveCaption";
constexpr char kVideoConferenceHistogramPrefix[] = "Ash.VideoConferenceTray";

}  // namespace

std::string GetEffectHistogramName(VcEffectId effect_id) {
  std::string effect_name;
  switch (effect_id) {
    case VcEffectId::kTestEffect:
      effect_name = kTestEffectHistogramName;
      break;
    case VcEffectId::kBackgroundBlur:
      effect_name = kBackgroundBlurHistogramName;
      break;
    case VcEffectId::kPortraitRelighting:
      effect_name = kPortraitRelightingHistogramName;
      break;
    case VcEffectId::kNoiseCancellation:
      effect_name = kNoiseCancellationHistogramName;
      break;
    case VcEffectId::kLiveCaption:
      effect_name = kLiveCaptionHistogramName;
      break;
  }
  return base::JoinString(
      {kVideoConferenceHistogramPrefix, effect_name, "Click"},
      /*separator=*/".");
}

}  // namespace ash::video_conference_utils