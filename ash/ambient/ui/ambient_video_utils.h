// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_UI_AMBIENT_VIDEO_UTILS_H_
#define ASH_AMBIENT_UI_AMBIENT_VIDEO_UTILS_H_

#include "ash/constants/ambient_video.h"

namespace base {
class FilePath;
}  // namespace base

namespace ash {

// Returns the full path to the specified |video|.
base::FilePath GetAmbientVideoPath(AmbientVideo video);

}  // namespace ash

#endif  // ASH_AMBIENT_UI_AMBIENT_VIDEO_UTILS_H_
