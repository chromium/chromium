// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/float/float_test_api.h"

#include "ash/shell.h"

namespace ash {

FloatTestApi::ScopedTuckEducationDisabler::ScopedTuckEducationDisabler() {
  Shell::Get()->float_controller()->disable_tuck_education_for_testing_ = true;
}

FloatTestApi::ScopedTuckEducationDisabler::~ScopedTuckEducationDisabler() {
  Shell::Get()->float_controller()->disable_tuck_education_for_testing_ = false;
}

// static
int FloatTestApi::GetFloatedWindowCounter() {
  return Shell::Get()->float_controller()->floated_window_counter_;
}

// static
int FloatTestApi::GetFloatedWindowMoveToAnotherDeskCounter() {
  return Shell::Get()
      ->float_controller()
      ->floated_window_move_to_another_desk_counter_;
}

// static
FloatController::MagnetismCorner FloatTestApi::GetMagnetismCornerForBounds(
    const gfx::Rect& bounds_in_screen) {
  return Shell::Get()->float_controller()->GetMagnetismCornerForBounds(
      bounds_in_screen);
}

}  // namespace ash
