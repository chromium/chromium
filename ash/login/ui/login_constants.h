// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOGIN_CONSTANTS_H_
#define ASH_LOGIN_UI_LOGIN_CONSTANTS_H_

namespace ash {
namespace login {

// Per above, the background should be a rounded rect with this corner radius.
constexpr int kNonBlurredWallpaperBackgroundRadiusDp = 4;

// How long should animations that change the layout of the user run for? For
// example, this includes the user switch animation as well as the PIN keyboard
// show/hide animation.
constexpr int kChangeUserAnimationDurationMs = 300;

}  // namespace login
}  // namespace ash

#endif  // ASH_LOGIN_UI_LOGIN_CONSTANTS_H_
