// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_AMBIENT_AMBIENT_MODE_PHOTO_SOURCE_H_
#define ASH_PUBLIC_CPP_AMBIENT_AMBIENT_MODE_PHOTO_SOURCE_H_

#include "ash/public/cpp/ash_public_export.h"

namespace ash {
namespace ambient {

// These values are persisted in UMA logs, so they should never be renumbered or
// reused. Used for reporting the histogram |Ash.AmbientMode.PhotoSource|.
enum class ASH_PUBLIC_EXPORT AmbientModePhotoSource {
  kUnset = 0,
  kArtGallery = 1,
  kGooglePhotosRecentHighlights = 2,
  kGooglePhotosPersonalAlbum = 3,
  kGooglePhotosBoth = 4,
  kGooglePhotosEmpty = 5,
  kMaxValue = kGooglePhotosEmpty,
};

}  // namespace ambient
}  // namespace ash

#endif  // ASH_PUBLIC_CPP_AMBIENT_AMBIENT_MODE_PHOTO_SOURCE_H_
