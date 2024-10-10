// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_BIRCH_BIRCH_ANIMATION_UTILS_H_
#define ASH_WM_OVERVIEW_BIRCH_BIRCH_ANIMATION_UTILS_H_

#include "ash/system/mahi/mahi_constants.h"
#include "cc/paint/skottie_wrapper.h"
#include "ui/lottie/animation.h"

namespace ash::birch_animation_utils {

// Gets lottie animation data from the json file that maps to the provided
// `animation_id`.
std::unique_ptr<lottie::Animation> GetLottieAnimationData(int animation_id);

// Gets the playback config for the animation that maps to the provided
// `animation_id`.
std::optional<lottie::Animation::PlaybackConfig> GetLottiePlaybackConfig(
    const cc::SkottieWrapper& skottie,
    int animation_id);

}  // namespace ash::birch_animation_utils

#endif  // ASH_WM_OVERVIEW_BIRCH_BIRCH_ANIMATION_UTILS_H_
