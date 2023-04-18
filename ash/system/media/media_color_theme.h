// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MEDIA_MEDIA_COLOR_THEME_H_
#define ASH_SYSTEM_MEDIA_MEDIA_COLOR_THEME_H_

#include "ash/ash_export.h"
#include "components/media_message_center/notification_theme.h"

namespace ash {

// Helper function that returns the common media color theme for Chrome OS.
ASH_EXPORT media_message_center::MediaColorTheme GetCrosMediaColorTheme();

}  // namespace ash

#endif  // ASH_SYSTEM_MEDIA_MEDIA_COLOR_THEME_H_
