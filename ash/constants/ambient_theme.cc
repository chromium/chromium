// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ambient_theme.h"

namespace ash {

base::StringPiece ToString(AmbientTheme theme) {
  // See the "AmbientModeThemes" <variants> tag in histograms.xml. These names
  // are currently used for metrics purposes, so they cannot be arbitrarily
  // renamed.
  switch (theme) {
    case AmbientTheme::kSlideshow:
      return "SlideShow";
    case AmbientTheme::kFeelTheBreeze:
      return "FeelTheBreeze";
    case AmbientTheme::kFloatOnBy:
      return "FloatOnBy";
    case AmbientTheme::kVideo:
      return "Video";
  }
}

}  // namespace ash
