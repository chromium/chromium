// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_AMBIENT_AMBIENT_METRICS_H_
#define ASH_PUBLIC_CPP_AMBIENT_AMBIENT_METRICS_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/time/time.h"

namespace ash {

struct AmbientSettings;
enum class AmbientUiMode;

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

ASH_PUBLIC_EXPORT AmbientModePhotoSource
AmbientSettingsToPhotoSource(const AmbientSettings& settings);

ASH_PUBLIC_EXPORT void RecordAmbientModeActivation(AmbientUiMode ui_mode,
                                                   bool tablet_mode);

ASH_PUBLIC_EXPORT void RecordAmbientModeTimeElapsed(base::TimeDelta time_delta,
                                                    bool tablet_mode);

}  // namespace ambient
}  // namespace ash

#endif  // ASH_PUBLIC_CPP_AMBIENT_AMBIENT_METRICS_H_
