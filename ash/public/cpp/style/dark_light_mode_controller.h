// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_STYLE_DARK_LIGHT_MODE_CONTROLLER_H_
#define ASH_PUBLIC_CPP_STYLE_DARK_LIGHT_MODE_CONTROLLER_H_

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

// An interface implemented by Ash that controls the behavior of dark/light
// mode. See DarkLightModeControllerImpl for more details.
class ASH_PUBLIC_EXPORT DarkLightModeController {
 public:
  static DarkLightModeController* Get();

 protected:
  DarkLightModeController();
  virtual ~DarkLightModeController();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_STYLE_DARK_LIGHT_MODE_CONTROLLER_H_
