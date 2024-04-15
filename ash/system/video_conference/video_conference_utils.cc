// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/video_conference_utils.h"

#include <string>

#include "ash/system/video_conference/effects/video_conference_tray_effects_manager_types.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/crosapi/mojom/video_conference.mojom.h"

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

std::u16string GetMediaAppDisplayText(
    const mojo::StructPtr<crosapi::mojom::VideoConferenceMediaAppInfo>&
        media_app) {
  auto url = media_app->url;
  auto title = media_app->title;

  // Displays the title if it is not empty. Otherwise, display app url.
  if (!title.empty()) {
    return title;
  }

  if (url) {
    return base::UTF8ToUTF16(url->GetContent());
  }

  return std::u16string();
}

}  // namespace ash::video_conference_utils