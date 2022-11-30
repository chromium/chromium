// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/style/scoped_light_mode_as_default.h"

#include "ash/style/dark_light_mode_controller_impl.h"

namespace ash {

ScopedLightModeAsDefault::ScopedLightModeAsDefault()
    : previous_override_light_mode_as_default_(
          DarkLightModeControllerImpl::Get()->override_light_mode_as_default_) {
  DarkLightModeControllerImpl::Get()->override_light_mode_as_default_ = true;
}

ScopedLightModeAsDefault::~ScopedLightModeAsDefault() {
  DarkLightModeControllerImpl::Get()->override_light_mode_as_default_ =
      previous_override_light_mode_as_default_;
}

}  // namespace ash
