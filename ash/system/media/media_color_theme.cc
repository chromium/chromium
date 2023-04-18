// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/media/media_color_theme.h"

#include "ash/style/ash_color_id.h"

namespace ash {

media_message_center::MediaColorTheme GetCrosMediaColorTheme() {
  media_message_center::MediaColorTheme theme;
  theme.primary_foreground_color_id = cros_tokens::kCrosSysOnSurface;
  theme.secondary_foreground_color_id = cros_tokens::kCrosSysSecondary;
  theme.primary_container_color_id = cros_tokens::kCrosSysPrimaryLight;
  theme.secondary_container_color_id =
      cros_tokens::kCrosSysSystemPrimaryContainer;
  theme.background_color_id = cros_tokens::kCrosSysSystemOnBase;
  return theme;
}

}  // namespace ash
