// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ambient_animation_theme.h"

namespace ash {

std::ostream& operator<<(std::ostream& os, AmbientAnimationTheme theme) {
  switch (theme) {
    case AmbientAnimationTheme::kSlideshow:
      return os << "SLIDESHOW";
    case AmbientAnimationTheme::kFeelTheBreeze:
      return os << "FEEL_THE_BREZE";
    case AmbientAnimationTheme::kFloatOnBy:
      return os << "FLOAT_ON_BY";
  }
}

}  // namespace ash
