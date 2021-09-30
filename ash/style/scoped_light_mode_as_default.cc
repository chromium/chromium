// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/style/scoped_light_mode_as_default.h"

#include "ash/constants/ash_features.h"
#include "ash/style/ash_color_provider.h"

namespace ash {

ScopedLightModeAsDefault::ScopedLightModeAsDefault()
    : previous_override_light_mode_as_default_(
          AshColorProvider::Get()->override_light_mode_as_default_) {
  AshColorProvider::Get()->override_light_mode_as_default_ = true;
}

ScopedLightModeAsDefault::~ScopedLightModeAsDefault() {
  AshColorProvider::Get()->override_light_mode_as_default_ =
      previous_override_light_mode_as_default_;
}

ScopedAssistantLightModeAsDefault::ScopedAssistantLightModeAsDefault()
    : previous_override_light_mode_as_default_(
          AshColorProvider::Get()->override_light_mode_as_default_) {
  if (!features::IsProductivityLauncherEnabled())
    AshColorProvider::Get()->override_light_mode_as_default_ = true;
}

ScopedAssistantLightModeAsDefault::~ScopedAssistantLightModeAsDefault() {
  AshColorProvider::Get()->override_light_mode_as_default_ =
      previous_override_light_mode_as_default_;
}

}  // namespace ash
