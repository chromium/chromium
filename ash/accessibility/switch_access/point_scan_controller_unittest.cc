// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/switch_access/point_scan_controller.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_window_builder.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/types/fixed_array.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/aura/window.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/gfx/image/image.h"
#include "ui/snapshot/snapshot.h"

namespace ash {

class PointScanControllerTest : public AshTestBase {
 public:
  PointScanControllerTest(const PointScanControllerTest&) = delete;
  PointScanControllerTest& operator=(const PointScanControllerTest&) = delete;

 protected:
  PointScanControllerTest() = default;
  ~PointScanControllerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ::switches::kEnablePixelOutputInTests);
    AshTestBase::SetUp();
  }

  void CaptureBeforeImage(const gfx::Rect& bounds) {
    Capture(bounds);
    if (before_bmp_.tryAllocPixels(image_.AsBitmap().info())) {
      image_.AsBitmap().readPixels(before_bmp_.info(), before_bmp_.getPixels(),
                                   before_bmp_.rowBytes(), 0, 0);
    }
  }

  void CaptureAfterImage(const gfx::Rect& bounds) {
    Capture(bounds);
    if (after_bmp_.tryAllocPixels(image_.AsBitmap().info())) {
      image_.AsBitmap().readPixels(after_bmp_.info(), after_bmp_.getPixels(),
                                   after_bmp_.rowBytes(), 0, 0);
    }
  }

  void Capture(const gfx::Rect& bounds) {
    // Occasionally we don't get any pixels the first try.
    // Keep trying until we get the correct size bitmap and the first pixel
    // is transparent.
    while (true) {
      aura::Window* window = Shell::GetPrimaryRootWindow();
      base::RunLoop run_loop;
      ui::GrabWindowSnapshotAndScale(
          window, bounds, bounds.size(),
          base::BindOnce(
              [](base::RunLoop* run_loop, gfx::Image* image,
                 gfx::Image got_image) {
                run_loop->Quit();
                *image = got_image;
              },
              &run_loop, &image_));
      run_loop.Run();
      SkBitmap bitmap = image_.AsBitmap();
      if (bitmap.width() != bounds.width() ||
          bitmap.height() != bounds.height()) {
        LOG(INFO) << "Bitmap not correct size, trying to capture again";
        continue;
      } else if (255 == SkColorGetA(bitmap.getColor(0, 0))) {
        LOG(INFO) << "Bitmap is transparent, trying to capture again";
        break;
      }
    }
  }

  void ComputeImageStats() {
    diff_count_ = 0;
    row_diff_count_ = 0;
    col_diff_count_ = 0;
    base::FixedArray<bool> row_diff_tracker_(before_bmp_.height(), false);
    for (int x = 0; x < before_bmp_.width(); ++x) {
      bool col_diff = false;
      for (int y = 0; y < before_bmp_.height(); ++y) {
        SkColor before_color = before_bmp_.getColor(x, y);
        SkColor after_color = after_bmp_.getColor(x, y);
        if (before_color != after_color) {
          diff_count_++;
          col_diff = true;
          row_diff_tracker_[y] = true;
        }
      }
      if (col_diff)
        ++col_diff_count_;
    }

    for (int i = 0; i < before_bmp_.height(); ++i) {
      if (row_diff_tracker_[i])
        ++row_diff_count_;
    }
  }

  int diff_count() const { return diff_count_; }
  int row_diff_count() const { return row_diff_count_; }
  int col_diff_count() const { return col_diff_count_; }

 private:
  gfx::Image image_;
  SkBitmap before_bmp_;
  SkBitmap after_bmp_;
  int diff_count_ = 0;
  int row_diff_count_ = 0;
  int col_diff_count_ = 0;
};

TEST_F(PointScanControllerTest, StartScanning) {
  gfx::Rect bounds = Shell::GetPrimaryRootWindow()->bounds();

  // Create a white background window for captured image color smoke test.
  std::unique_ptr<aura::Window> window =
      TestWindowBuilder()
          .SetColorWindowDelegate(SK_ColorWHITE)
          .SetBounds(bounds)
          .Build();

  // For some reason, the white window is not fully drawn the first time we call
  // CaptureBeforeImage.
  CaptureBeforeImage(bounds);
  CaptureBeforeImage(bounds);

  PointScanController controller;
  controller.StartHorizontalRangeScan();

  CaptureAfterImage(bounds);
  ComputeImageStats();
}

}  // namespace ash
