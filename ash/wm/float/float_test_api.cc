// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/float/float_test_api.h"

#include "ash/shell.h"
#include "ash/wm/float/float_controller.h"

namespace ash {

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

}  // namespace ash
