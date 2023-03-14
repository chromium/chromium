// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_MODEL_AMBIENT_SLIDESHOW_PHOTO_CONFIG_H_
#define ASH_AMBIENT_MODEL_AMBIENT_SLIDESHOW_PHOTO_CONFIG_H_

#include "ash/ambient/model/ambient_photo_config.h"
#include "ash/ash_export.h"

namespace ash {

// For the UI that iterates through a slideshow of images and displays them at
// full-screen resolution.
ASH_EXPORT AmbientPhotoConfig CreateAmbientSlideshowPhotoConfig();

// For the UI that iterates through an admin provided slideshow of images and
// displays them at full-screen resolution.
ASH_EXPORT AmbientPhotoConfig CreateAmbientManagedSlideshowPhotoConfig();

}  // namespace ash

#endif  // ASH_AMBIENT_MODEL_AMBIENT_SLIDESHOW_PHOTO_CONFIG_H_
