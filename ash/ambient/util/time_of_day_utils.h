// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_UTIL_TIME_OF_DAY_UTILS_H_
#define ASH_AMBIENT_UTIL_TIME_OF_DAY_UTILS_H_

#include "ash/ash_export.h"
#include "base/files/file_path.h"

namespace ash {

// Returns paths to assets required for the TimeOfDay wallpaper/screensaver
// feature.
ASH_EXPORT const base::FilePath& GetTimeOfDaySrcDir();

// TimeOfDay video file names.
ASH_EXPORT extern const base::FilePath::CharType kTimeOfDayCloudsVideo[];
ASH_EXPORT extern const base::FilePath::CharType kTimeOfDayNewMexicoVideo[];

// HTML that renders that TimeOfDay video.
ASH_EXPORT extern const base::FilePath::CharType kAmbientVideoHtml[];

}  // namespace ash

#endif  // ASH_AMBIENT_UTIL_TIME_OF_DAY_UTILS_H_
