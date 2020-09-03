// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_TEST_CAPTURE_MODE_DELEGATE_H_
#define ASH_CAPTURE_MODE_TEST_CAPTURE_MODE_DELEGATE_H_

#include "ash/public/cpp/capture_mode_delegate.h"

namespace ash {

class TestCaptureModeDelegate : public CaptureModeDelegate {
 public:
  TestCaptureModeDelegate() = default;
  TestCaptureModeDelegate(const TestCaptureModeDelegate&) = delete;
  TestCaptureModeDelegate& operator=(const TestCaptureModeDelegate&) = delete;
  ~TestCaptureModeDelegate() override = default;

  // CaptureModeDelegate:
  base::FilePath GetActiveUserDownloadsDir() const override;
  void ShowScreenCaptureItemInFolder(const base::FilePath& file_path) override;
  bool Uses24HourFormat() const override;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_TEST_CAPTURE_MODE_DELEGATE_H_
