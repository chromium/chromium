// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/raster_scale_controller.h"

#include <vector>

#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/raster_scale_change_tracker.h"
#include "base/memory/raw_ptr.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/test_windows.h"

namespace ash {

using RasterScaleControllerTest = AshTestBase;

TEST_F(RasterScaleControllerTest, RasterScaleOnlyUpdatesIfChanges) {
  std::unique_ptr<aura::Window> window(CreateTestWindow(gfx::Rect(100, 100)));
  auto tracker = RasterScaleChangeTracker(window.get());

  EXPECT_EQ(1.0f, window->GetProperty(aura::client::kRasterScale));
  EXPECT_EQ(std::vector<float>{}, tracker.TakeRasterScaleChanges());

  {
    ScopedSetRasterScale scoped1(window.get(), 2.0f);
    EXPECT_EQ(2.0f, window->GetProperty(aura::client::kRasterScale));
    EXPECT_EQ(std::vector<float>{2.0f}, tracker.TakeRasterScaleChanges());

    {
      ScopedSetRasterScale scoped2(window.get(), 2.0f);

      // The raster scale didn't change, so expect no raster scale changes to be
      // sent.
      EXPECT_EQ(2.0f, window->GetProperty(aura::client::kRasterScale));
      EXPECT_EQ(std::vector<float>{}, tracker.TakeRasterScaleChanges());
    }

    // Removing the same scale (2.0f) once should still leave the existing 2.0f
    // there.
    EXPECT_EQ(2.0f, window->GetProperty(aura::client::kRasterScale));
    EXPECT_EQ(std::vector<float>{}, tracker.TakeRasterScaleChanges());
  }
  EXPECT_EQ(1.0f, window->GetProperty(aura::client::kRasterScale));
  EXPECT_EQ(std::vector<float>{1.0f}, tracker.TakeRasterScaleChanges());
}

TEST_F(RasterScaleControllerTest, RasterScalePause) {
  std::unique_ptr<aura::Window> window(CreateTestWindow(gfx::Rect(100, 100)));
  auto tracker = RasterScaleChangeTracker(window.get());

  EXPECT_EQ(1.0f, window->GetProperty(aura::client::kRasterScale));
  EXPECT_EQ(std::vector<float>{}, tracker.TakeRasterScaleChanges());

  auto scoped_pause = std::make_unique<ScopedPauseRasterScaleUpdates>();

  {
    ScopedSetRasterScale scoped1(window.get(), 2.0f);

    // Since updates are paused, expect nothing to change.
    EXPECT_EQ(1.0f, window->GetProperty(aura::client::kRasterScale));
    EXPECT_EQ(std::vector<float>{}, tracker.TakeRasterScaleChanges());

    // Unpausing should set it to 2.0f now.
    scoped_pause.reset();
    EXPECT_EQ(2.0f, window->GetProperty(aura::client::kRasterScale));
    EXPECT_EQ(std::vector<float>{2.0f}, tracker.TakeRasterScaleChanges());

    // Pause again, and then destroy scoped1.
    scoped_pause = std::make_unique<ScopedPauseRasterScaleUpdates>();
  }

  // It should still be at 2.0f since we are paused.
  EXPECT_EQ(2.0f, window->GetProperty(aura::client::kRasterScale));
  EXPECT_EQ(std::vector<float>{}, tracker.TakeRasterScaleChanges());

  // Unpausing should set it to 1.0f now.
  scoped_pause.reset();
  EXPECT_EQ(1.0f, window->GetProperty(aura::client::kRasterScale));
  EXPECT_EQ(std::vector<float>{1.0f}, tracker.TakeRasterScaleChanges());
}

}  // namespace ash
