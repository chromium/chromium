// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_RESTORE_ARC_WINDOW_UTILS_H_
#define CHROME_BROWSER_ASH_APP_RESTORE_ARC_WINDOW_UTILS_H_

#include <optional>

#include "components/services/app_service/public/cpp/app_launch_util.h"

namespace ash::full_restore {

std::optional<double> GetDisplayScaleFactor(int64_t display_id);

// Returns true if the ARC supports ghost window.
bool IsArcGhostWindowEnabled();

// Returns window info compatible with ARC. If the window bounds is not
// appropriate for the display, it will be removed.
//
// The app window bounds can be decided if and only if it matches the
// conditions:
//   1. The |display_id| still exists on system.
//   2. Previous ARC app window bounds on display is recorded.
// Otherwise returns null.
apps::WindowInfoPtr HandleArcWindowInfo(apps::WindowInfoPtr window_info);

// Returns true if it is a valid theme color. In Android, any transparent color
// cannot be a topic color.
bool IsValidThemeColor(uint32_t theme_color);

const std::string WrapSessionAppIdFromWindowId(int window_id);

}  // namespace ash::full_restore

#endif  // CHROME_BROWSER_ASH_APP_RESTORE_ARC_WINDOW_UTILS_H_
