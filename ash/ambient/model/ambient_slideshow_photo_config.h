// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_MODEL_AMBIENT_SLIDESHOW_PHOTO_CONFIG_H_
#define ASH_AMBIENT_MODEL_AMBIENT_SLIDESHOW_PHOTO_CONFIG_H_

#include "ash/ambient/model/ambient_photo_config.h"

namespace ash {

// For the UI that iterates through a slideshow of images and displays them at
// full-screen resolution.
constexpr AmbientPhotoConfig kAmbientSlideshowPhotoConfig = {
    // The UI can render both the primary and related photo at the same time in
    // certain cases (ex: 2 portrait photos displayed on the left and right
    // halves of the screen).
    /*should_split_topics=*/false,
    // Always having 2 topics available prevents any chance of screen burn. In
    // the worst case scenario that no more assets become available, the
    // slideshow can alternate between the 2 topics indefinitely.
    /*num_decoded_topics_to_buffer=*/2,
};

}  // namespace ash

#endif  // ASH_AMBIENT_MODEL_AMBIENT_SLIDESHOW_PHOTO_CONFIG_H_
