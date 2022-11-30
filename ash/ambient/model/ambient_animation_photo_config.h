// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_MODEL_AMBIENT_ANIMATION_PHOTO_CONFIG_H_
#define ASH_AMBIENT_MODEL_AMBIENT_ANIMATION_PHOTO_CONFIG_H_

#include "ash/ambient/model/ambient_photo_config.h"
#include "ash/ash_export.h"

namespace cc {
class SkottieResourceMetadataMap;
}  // namespace cc

namespace ash {

// For the UI that renders a Lottie animation file using the Skottie library.
// The Lottie animation file has image assets embedded in it; the location and
// number of assets varies from file to file and depends on the animation theme
// (built by a motion designer).
ASH_EXPORT AmbientPhotoConfig CreateAmbientAnimationPhotoConfig(
    const cc::SkottieResourceMetadataMap& skottie_resource_metadata);

}  // namespace ash

#endif  // ASH_AMBIENT_MODEL_AMBIENT_ANIMATION_PHOTO_CONFIG_H_
