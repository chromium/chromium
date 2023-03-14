// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GLANCEABLES_GLANCEABLES_UTIL_H_
#define ASH_GLANCEABLES_GLANCEABLES_UTIL_H_

namespace base {
class FilePath;
}  // namespace base

namespace ash::glanceables_util {

// Returns the path to the signout screenshot, for example
// /home/chronos/u-<hash>/signout_screenshot.png
base::FilePath GetSignoutScreenshotPath();

}  // namespace ash::glanceables_util

#endif  // ASH_GLANCEABLES_GLANCEABLES_UTIL_H_
