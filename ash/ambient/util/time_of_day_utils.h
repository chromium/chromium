// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_UTIL_TIME_OF_DAY_UTILS_H_
#define ASH_AMBIENT_UTIL_TIME_OF_DAY_UTILS_H_

#include "ash/ash_export.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"

namespace ash {

// Returns paths to assets required for the TimeOfDay wallpaper/screensaver
// feature.
// TODO(b/289085706): Integrate DLC into this function.
ASH_EXPORT const base::FilePath& GetTimeOfDaySrcDir();

// Installs the TimeOfDay DLC package containing assets for the
// Time Of Day screen saver. DLC will eventually replace the Time Of Day assets
// currently stored in rootfs. Returns the root directory where the assets are
// located. Returns an empty `base::FilePath` if the install fails.
ASH_EXPORT void InstallTimeOfDayDlc(
    base::OnceCallback<void(base::FilePath)> on_done);

// TimeOfDay video file names.
ASH_EXPORT extern const base::FilePath::CharType kTimeOfDayCloudsVideo[];
ASH_EXPORT extern const base::FilePath::CharType kTimeOfDayNewMexicoVideo[];

// HTML that renders that TimeOfDay video.
ASH_EXPORT extern const base::FilePath::CharType kAmbientVideoHtml[];

}  // namespace ash

#endif  // ASH_AMBIENT_UTIL_TIME_OF_DAY_UTILS_H_
