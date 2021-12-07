// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/model/ambient_slideshow_photo_config.h"

#include "ash/ambient/model/ambient_backend_model.h"

namespace ash {

// Slideshow screensaver only has 1 full-screen asset. Primary/related photos
// can be split within the asset in certain scenarios (ex: 2 portrait photos
// displayed on the left and right halves of the screen).

namespace {

constexpr int kTotalNumAssets = 1;
// Always having 2 sets of assets available prevents any chance of screen
// burn. In the worst case scenario that no more assets become available, the
// slideshow can alternate between the 2 assets indefinitely.
constexpr int kNumSetsToBuffer = 2;

}  // namespace

AmbientSlideshowPhotoConfig::AmbientSlideshowPhotoConfig() = default;

AmbientSlideshowPhotoConfig::~AmbientSlideshowPhotoConfig() = default;

int AmbientSlideshowPhotoConfig::GetNumAssets() const {
  return kTotalNumAssets;
}

int AmbientSlideshowPhotoConfig::GetNumSetsOfAssetsToBuffer() const {
  return kNumSetsToBuffer;
}

int AmbientSlideshowPhotoConfig::GetNumAssetsInTopic(
    const PhotoWithDetails& decoded_topic) const {
  // If the related photo is available, it can be displayed within the same
  // asset as the primary photo, so ultimately, at most 1 asset is occupied.
  return decoded_topic.IsNull() ? 0 : kTotalNumAssets;
}

}  // namespace ash
