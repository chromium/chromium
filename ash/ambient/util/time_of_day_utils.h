// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_UTIL_TIME_OF_DAY_UTILS_H_
#define ASH_AMBIENT_UTIL_TIME_OF_DAY_UTILS_H_

#include "ash/ash_export.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"

namespace ash {

// Returns full path to the html required to render the TimeOfDay screen saver.
// The returned path will be empty if an error occurred and the html is
// temporarily unavailable.
ASH_EXPORT void GetAmbientVideoHtmlPath(
    base::OnceCallback<void(base::FilePath)> on_done);

// TimeOfDay video file names.
ASH_EXPORT extern const base::FilePath::CharType kTimeOfDayCloudsVideo[];
ASH_EXPORT extern const base::FilePath::CharType kTimeOfDayNewMexicoVideo[];
ASH_EXPORT extern const base::FilePath::CharType
    kTimeOfDayAssetsRootfsRootDir[];
ASH_EXPORT extern const base::FilePath::CharType kTimeOfDayVideoHtmlSubPath[];

}  // namespace ash

#endif  // ASH_AMBIENT_UTIL_TIME_OF_DAY_UTILS_H_
