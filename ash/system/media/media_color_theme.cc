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

  // Colors for the play/pause button.
  theme.play_button_foreground_color_id = cros_tokens::kCrosSysSecondary;
  theme.play_button_container_color_id =
      cros_tokens::kCrosSysRippleNeutralOnSubtle;
  theme.pause_button_foreground_color_id =
      cros_tokens::kCrosSysSystemOnPrimaryContainer;
  theme.pause_button_container_color_id =
      cros_tokens::kCrosSysSystemPrimaryContainer;

  // Colors for the progress view.
  theme.playing_progress_foreground_color_id = cros_tokens::kCrosSysPrimary;
  theme.playing_progress_background_color_id =
      cros_tokens::kCrosSysHighlightShape;
  theme.paused_progress_foreground_color_id = cros_tokens::kCrosSysSecondary;
  theme.paused_progress_background_color_id =
      cros_tokens::kCrosSysHoverOnSubtle;

  theme.background_color_id = cros_tokens::kCrosSysSystemOnBase;
  theme.separator_color_id = cros_tokens::kCrosSysSeparator;
  theme.error_foreground_color_id = cros_tokens::kCrosSysError;
  theme.error_container_color_id = cros_tokens::kCrosSysErrorContainer;
  theme.focus_ring_color_id = cros_tokens::kCrosSysFocusRing;
  return theme;
}

}  // namespace ash
