// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/raster_scale/raster_scale_controller.h"

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

// Tests that raster scale is only updated when it changes by a slop proportion.
TEST_F(RasterScaleControllerTest, RasterScaleSlop) {
  std::unique_ptr<aura::Window> window(CreateTestWindow(gfx::Rect(100, 100)));
  auto tracker = RasterScaleChangeTracker(window.get());

  EXPECT_EQ(1.0f, window->GetProperty(aura::client::kRasterScale));
  EXPECT_EQ(std::vector<float>{}, tracker.TakeRasterScaleChanges());

  const auto slop =
      Shell::Get()->raster_scale_controller()->raster_scale_slop_proportion();

  {
    const auto target_scale = 1.0f + slop * 2.0f;
    ScopedSetRasterScale scoped1(window.get(), target_scale);

    // A 100% change should trigger a change.
    EXPECT_EQ(target_scale, window->GetProperty(aura::client::kRasterScale));
    EXPECT_EQ(std::vector<float>{target_scale},
              tracker.TakeRasterScaleChanges());
  }

  // Should go back to 1.0f.
  EXPECT_EQ(1.0f, window->GetProperty(aura::client::kRasterScale));
  EXPECT_EQ(std::vector<float>{1.0f}, tracker.TakeRasterScaleChanges());

  {
    const auto target_scale = 1.0f + slop / 2.0f;
    ScopedSetRasterScale scoped1(window.get(), target_scale);

    // A change of lower proportion than `slop` should not trigger
    // a change.
    EXPECT_EQ(1.0f, window->GetProperty(aura::client::kRasterScale));
    EXPECT_EQ(std::vector<float>{}, tracker.TakeRasterScaleChanges());
  }

  // Expect no changes after scope ends.
  EXPECT_EQ(1.0f, window->GetProperty(aura::client::kRasterScale));
  EXPECT_EQ(std::vector<float>{}, tracker.TakeRasterScaleChanges());

  const auto target_scale_h2 = 1.0f + slop / 2.0f;
  {
    ScopedSetRasterScale scoped1(window.get(), target_scale_h2);

    // A change of lower proportion than `slop` should not trigger
    // a change.
    EXPECT_EQ(1.0f, window->GetProperty(aura::client::kRasterScale));
    EXPECT_EQ(std::vector<float>{}, tracker.TakeRasterScaleChanges());

    {
      const auto target_scale_x2 = 1.0f + slop * 2.0f;
      ScopedSetRasterScale scoped2(window.get(), target_scale_x2);

      // Should update to `target_scale_x2`.
      EXPECT_EQ(target_scale_x2,
                window->GetProperty(aura::client::kRasterScale));
      EXPECT_EQ(std::vector<float>{target_scale_x2},
                tracker.TakeRasterScaleChanges());
    }

    // After releasing `scoped2`, the change from `target_scale_x2` to
    // `target_scale_h2` should exceed the raster slop, so expect an update.
    EXPECT_EQ(target_scale_h2, window->GetProperty(aura::client::kRasterScale));
    EXPECT_EQ(std::vector<float>{target_scale_h2},
              tracker.TakeRasterScaleChanges());
  }

  // Even though the change from `target_scale_h2` to 1.0f should be less
  // than the raster scale slop by proportion, this should be special cased to
  // return it to 1.0f. See comments on `slop`.
  EXPECT_LE((target_scale_h2 - 1.0f) / target_scale_h2, slop);
  EXPECT_EQ(1.0f, window->GetProperty(aura::client::kRasterScale));
  EXPECT_EQ(std::vector<float>{1.0f}, tracker.TakeRasterScaleChanges());
}

// Tests that raster scale is not reduced below `kMinimumRasterScale`.
TEST_F(RasterScaleControllerTest, RasterScaleMinimumValue) {
  std::unique_ptr<aura::Window> window(CreateTestWindow(gfx::Rect(100, 100)));
  auto tracker = RasterScaleChangeTracker(window.get());

  EXPECT_EQ(1.0f, window->GetProperty(aura::client::kRasterScale));
  EXPECT_EQ(std::vector<float>{}, tracker.TakeRasterScaleChanges());

  {
    // Expect to be able to set raster scale down to the minimum value.
    const auto target_scale = RasterScaleController::kMinimumRasterScale;
    ScopedSetRasterScale scoped1(window.get(), target_scale);
    EXPECT_EQ(target_scale, window->GetProperty(aura::client::kRasterScale));
    EXPECT_EQ(std::vector<float>{target_scale},
              tracker.TakeRasterScaleChanges());
  }

  EXPECT_EQ(1.0f, window->GetProperty(aura::client::kRasterScale));
  EXPECT_EQ(std::vector<float>{1.0f}, tracker.TakeRasterScaleChanges());

  {
    // Expect raster scale to not be able to be set below the minimum value.
    const auto target_scale = RasterScaleController::kMinimumRasterScale * 0.9f;
    ScopedSetRasterScale scoped1(window.get(), target_scale);
    EXPECT_EQ(RasterScaleController::kMinimumRasterScale,
              window->GetProperty(aura::client::kRasterScale));
    EXPECT_EQ(std::vector<float>{RasterScaleController::kMinimumRasterScale},
              tracker.TakeRasterScaleChanges());
  }
}

}  // namespace ash
