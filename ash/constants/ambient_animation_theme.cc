// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ambient_animation_theme.h"

namespace ash {

base::StringPiece ToString(AmbientAnimationTheme theme) {
  // See the "AmbientModeThemes" <variants> tag in histograms.xml. These names
  // are currently used for metrics purposes, so they cannot be arbitrarily
  // renamed.
  switch (theme) {
    case AmbientAnimationTheme::kSlideshow:
      return "SlideShow";
    case AmbientAnimationTheme::kFeelTheBreeze:
      return "FeelTheBreeze";
    case AmbientAnimationTheme::kFloatOnBy:
      return "FloatOnBy";
  }
}

}  // namespace ash
