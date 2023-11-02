// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SCREEN_BACKLIGHT_TYPE_H_
#define ASH_PUBLIC_CPP_SCREEN_BACKLIGHT_TYPE_H_

namespace ash {

// Screen backlight state as communicated by D-Bus signals from powerd about
// backlight brightness changes.
enum class ScreenBacklightState {
  // The screen is on.
  ON,
  // The screen is off.
  OFF,
  // The screen is off, specifically due to an automated change like user
  // inactivity.
  OFF_AUTO,
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SCREEN_BACKLIGHT_TYPE_H_
