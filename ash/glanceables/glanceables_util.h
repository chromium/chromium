// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GLANCEABLES_GLANCEABLES_UTIL_H_
#define ASH_GLANCEABLES_GLANCEABLES_UTIL_H_

#include "ash/ash_export.h"

namespace base {
class FilePath;
}  // namespace base

namespace ash::glanceables_util {

// Returns the path to the signout screenshot, for example
// /home/chronos/u-<hash>/signout_screenshot.png
ASH_EXPORT base::FilePath GetSignoutScreenshotPath();

// Removes signout screenshot located at `GetSignoutScreenshotPath()`.
ASH_EXPORT void DeleteScreenshot();

}  // namespace ash::glanceables_util

#endif  // ASH_GLANCEABLES_GLANCEABLES_UTIL_H_
