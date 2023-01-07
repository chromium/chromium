// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_CAPTURE_MODE_CAPTURE_MODE_API_H_
#define ASH_PUBLIC_CPP_CAPTURE_MODE_CAPTURE_MODE_API_H_

#include "ash/ash_export.h"

namespace ash {

// Full screen capture for each available display if no restricted content
// exists on that display, each capture is saved as an individual file.
// Note: this won't start a capture mode session.
void ASH_EXPORT CaptureScreenshotsOfAllDisplays();

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_CAPTURE_MODE_CAPTURE_MODE_API_H_
