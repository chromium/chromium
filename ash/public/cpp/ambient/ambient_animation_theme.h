// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_AMBIENT_AMBIENT_ANIMATION_THEME_H_
#define ASH_PUBLIC_CPP_AMBIENT_AMBIENT_ANIMATION_THEME_H_

#include <ostream>

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

// Each corresponds to an animation design for ambient mode that UX created and
// has its own Lottie file.
//
// These values are persisted in user pref storage, so they should never be
// renumbered or reused.
enum class ASH_PUBLIC_EXPORT AmbientAnimationTheme {
  // This is the one exception in the list, and it describes the mode where
  // IMAX photos are displayed at full screen in a slideshow fashion. This is
  // not currently implemented as an "animation" and doesn't have a Lottie file.
  // It is currently implemented entirely as a native UI view.
  kSlideshow = 0,
  kFeelTheBreeze = 1,
};

ASH_PUBLIC_EXPORT std::ostream& operator<<(std::ostream& os,
                                           AmbientAnimationTheme theme);

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_AMBIENT_AMBIENT_ANIMATION_THEME_H_
