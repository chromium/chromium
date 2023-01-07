// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CONSTANTS_AMBIENT_ANIMATION_THEME_H_
#define ASH_CONSTANTS_AMBIENT_ANIMATION_THEME_H_

#include "base/component_export.h"
#include "base/strings/string_piece.h"

namespace ash {

// Each corresponds to an animation design for ambient mode that UX created and
// has its own Lottie file.
//
// These values are persisted in user pref storage and logs, so they should
// never be renumbered or reused.
enum class AmbientAnimationTheme {
  // This is the one exception in the list, and it describes the mode where
  // IMAX photos are displayed at full screen in a slideshow fashion. This is
  // not currently implemented as an "animation" and doesn't have a Lottie file.
  // It is currently implemented entirely as a native UI view.
  kSlideshow = 0,
  kFeelTheBreeze = 1,
  kFloatOnBy = 2,
  kMaxValue = kFloatOnBy,
};

inline constexpr AmbientAnimationTheme kDefaultAmbientAnimationTheme =
    AmbientAnimationTheme::kSlideshow;

// The returned StringPiece is guaranteed to be null-terminated and point to
// memory valid for the lifetime of the program.
COMPONENT_EXPORT(ASH_CONSTANTS)
base::StringPiece ToString(AmbientAnimationTheme theme);

}  // namespace ash

#endif  // ASH_CONSTANTS_AMBIENT_ANIMATION_THEME_H_
