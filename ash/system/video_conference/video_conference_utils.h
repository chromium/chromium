// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_VIDEO_CONFERENCE_VIDEO_CONFERENCE_UTILS_H_
#define ASH_SYSTEM_VIDEO_CONFERENCE_VIDEO_CONFERENCE_UTILS_H_

#include <string>

#include "ash/ash_export.h"
#include "cc/paint/skottie_color_map.h"
#include "chromeos/crosapi/mojom/video_conference.mojom-forward.h"
#include "ui/color/color_provider.h"

namespace ash {

enum class VcEffectId;

namespace video_conference_utils {

// Get the histogram name for the click histogram associated with `effect_id`.
ASH_EXPORT std::string GetEffectHistogramNameForClick(VcEffectId effect_id);

// Get the histogram name for the initial state histogram associated with
// `effect_id`.
std::string GetEffectHistogramNameForInitialState(VcEffectId effect_id);

// Gets the display text representing a media app shown in the return to app
// button.
std::u16string GetMediaAppDisplayText(
    const mojo::StructPtr<crosapi::mojom::VideoConferenceMediaAppInfo>&
        media_app);

// Lottie animation doesn't support dark mode color, in order to make the
// animation look good in both dark and light modes, we manually override the
// colors used in the animation.
cc::SkottieColorMap CreateColorMapForGradientAnimation(
    const ui::ColorProvider* color_provider);

}  // namespace video_conference_utils

}  // namespace ash

#endif  // ASH_SYSTEM_VIDEO_CONFERENCE_VIDEO_CONFERENCE_UTILS_H_
