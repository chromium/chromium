// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/home_screen/home_screen_controller.h"

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/shell.h"

namespace ash {

HomeScreenController::HomeScreenController() = default;

HomeScreenController::~HomeScreenController() = default;

bool HomeScreenController::GoHome(int64_t display_id) {
  return Shell::Get()->app_list_controller()->GoHome(display_id);
}

}  // namespace ash
