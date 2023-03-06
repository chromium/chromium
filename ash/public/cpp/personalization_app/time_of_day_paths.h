// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_PERSONALIZATION_APP_TIME_OF_DAY_PATHS_H_
#define ASH_PUBLIC_CPP_PERSONALIZATION_APP_TIME_OF_DAY_PATHS_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/files/file_path.h"

namespace ash::personalization_app {

// Returns paths to assets required for the TimeOfDay wallpaper/screensaver
// feature.
ASH_PUBLIC_EXPORT const base::FilePath& GetTimeOfDayWallpapersDir();
ASH_PUBLIC_EXPORT const base::FilePath& GetTimeOfDayVideosDir();
ASH_PUBLIC_EXPORT const base::FilePath& GetTimeOfDaySrcDir();

// TimeOfDay video file names.
ASH_PUBLIC_EXPORT extern const base::FilePath::CharType kTimeOfDayCloudsVideo[];
ASH_PUBLIC_EXPORT extern const base::FilePath::CharType
    kTimeOfDayNewMexicoVideo[];

// HTML that renders that TimeOfDay video.
ASH_PUBLIC_EXPORT extern const base::FilePath::CharType kAmbientVideoHtml[];

}  // namespace ash::personalization_app

#endif  // ASH_PUBLIC_CPP_PERSONALIZATION_APP_TIME_OF_DAY_PATHS_H_
