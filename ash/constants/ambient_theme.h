// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CONSTANTS_AMBIENT_THEME_H_
#define ASH_CONSTANTS_AMBIENT_THEME_H_

#include "base/component_export.h"
#include "base/strings/string_piece.h"

namespace ash {

// Each corresponds to a distinct UI in ambient mode that can be selected by the
// user.
//
// These values are persisted in user pref storage and logs, so they should
// never be renumbered or reused.
enum class AmbientTheme {
  // IMAX photos are displayed at full screen in a slideshow fashion. This is
  // currently implemented as a native UI view.
  kSlideshow = 0,
  // Implemented as Lottie animations, each of which has their own animation
  // file created by motion designers.
  kFeelTheBreeze = 1,
  kFloatOnBy = 2,
  // Scenic videos that get played on loop at full screen. The videos are static
  // and Google-owned.
  kVideoNewMexico = 3,
  kVideoClouds = 4,
  kMaxValue = kVideoClouds,
};

inline constexpr AmbientTheme kDefaultAmbientTheme = AmbientTheme::kSlideshow;

// The returned StringPiece is guaranteed to be null-terminated and point to
// memory valid for the lifetime of the program.
COMPONENT_EXPORT(ASH_CONSTANTS)
base::StringPiece ToString(AmbientTheme theme);

}  // namespace ash

#endif  // ASH_CONSTANTS_AMBIENT_THEME_H_
