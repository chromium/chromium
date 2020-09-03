// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/test_capture_mode_delegate.h"

#include "base/files/file_path.h"

namespace ash {

base::FilePath TestCaptureModeDelegate::GetActiveUserDownloadsDir() const {
  // TODO(afakhry): Add proper code to enable testing.
  return base::FilePath();
}

void TestCaptureModeDelegate::ShowScreenCaptureItemInFolder(
    const base::FilePath& file_path) {}

bool TestCaptureModeDelegate::Uses24HourFormat() const {
  return false;
}

}  // namespace ash
