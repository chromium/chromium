// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HOME_SCREEN_HOME_SCREEN_CONTROLLER_H_
#define ASH_HOME_SCREEN_HOME_SCREEN_CONTROLLER_H_

#include <stdint.h>

#include "ash/ash_export.h"
#include "base/macros.h"

namespace ash {

// HomeScreenController provides functionality to control the home launcher -
// the tablet mode app list.
// NOTE: This class is being folded into AppListControllerImpl. Its tests live
// in ash/app_list/app_list_controller_impl_unittest.cc.
class ASH_EXPORT HomeScreenController {
 public:
  HomeScreenController();
  ~HomeScreenController();

  // DEPRECATED: Use AppListControllerImpl::GoHome().
  bool GoHome(int64_t display_id);

 private:
  DISALLOW_COPY_AND_ASSIGN(HomeScreenController);
};

}  // namespace ash

#endif  // ASH_HOME_SCREEN_HOME_SCREEN_CONTROLLER_H_
