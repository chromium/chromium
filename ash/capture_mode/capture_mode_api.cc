// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/capture_mode/capture_mode_api.h"

#include "ash/capture_mode/capture_mode_controller.h"

namespace ash {

void CaptureScreenshotsOfAllDisplays() {
  CaptureModeController::Get()->CaptureScreenshotsOfAllDisplays();
}

}  // namespace ash
