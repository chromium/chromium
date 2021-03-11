// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_LOGIN_CONSTANTS_H_
#define ASH_PUBLIC_CPP_LOGIN_CONSTANTS_H_

#include "third_party/skia/include/core/SkColor.h"

// TODO(crbug/1148231): Move this file to ash/login/ui/login_constant.h.

namespace ash {
namespace login_constants {

// Per above, the background should be a rounded rect with this corner radius.
constexpr int kNonBlurredWallpaperBackgroundRadiusDp = 4;

// The blur sigma for login/lock screen.
constexpr float kBlurSigma = 30.0f;

// How long should animations that change the layout of the user run for? For
// example, this includes the user switch animation as well as the PIN keyboard
// show/hide animation.
constexpr int kChangeUserAnimationDurationMs = 300;

// The most used font size on login/lock screen.
constexpr int kDefaultFontSize = 13;

// The most used font on login/lock screen.
constexpr char kDefaultFontName[] = "Roboto";

}  // namespace login_constants
}  // namespace ash

#endif  // ASH_PUBLIC_CPP_LOGIN_CONSTANTS_H_
