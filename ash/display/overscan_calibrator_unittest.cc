// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/overscan_calibrator.h"

#include "ash/display/cros_display_config.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/stringprintf.h"
#include "ui/aura/window.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/managed_display_info.h"

namespace ash {

class OverscanCalibratorTest : public AshTestBase {
 public:
  OverscanCalibratorTest() = default;
  ~OverscanCalibratorTest() override = default;
  OverscanCalibratorTest(OverscanCalibratorTest&) = delete;
  OverscanCalibratorTest& operator=(const OverscanCalibratorTest&) = delete;

  OverscanCalibrator* StartCalibration(int64_t display_id) {
    Shell::Get()->cros_display_config()->OverscanCalibration(
        display_id, DisplayCalibrationOperation::kStart,
        gfx::Insets() /* not used */);
    return Shell::Get()->cros_display_config()->GetOverscanCalibrator(
        display_id);
  }
};

TEST_F(OverscanCalibratorTest, Rotation) {
  auto* display_manager = Shell::Get()->display_manager();

  int64_t display_id = display::Screen::Get()->GetPrimaryDisplay().id();

  auto* calibrator = StartCalibration(display_id);
  calibrator->UpdateInsets(gfx::Insets::TLBR(100, 5, 10, 15));
  calibrator->Commit();
  display::ManagedDisplayInfo info =
      display_manager->GetDisplayInfo(display_id);
  EXPECT_EQ(gfx::Insets::TLBR(100, 5, 10, 15), info.overscan_insets_in_dip());

  display_manager->SetDisplayRotation(display_id,
                                      display::Display::Rotation::ROTATE_90,
                                      display::Display::RotationSource::USER);
  EXPECT_EQ(gfx::Size(490, 780),
            display::Screen::Get()->GetPrimaryDisplay().size());

  calibrator = StartCalibration(display_id);
  // The insets will be rotated and applied in the host coordinates.
  gfx::Insets insets = calibrator->insets();
  insets.set_left(105);
  insets.set_top(0);
  calibrator->UpdateInsets(insets);
  calibrator->Commit();

  info = display_manager->GetDisplayInfo(display_id);
  EXPECT_EQ(gfx::Insets::TLBR(105, 5, 10, 0), info.overscan_insets_in_dip());
}

TEST_F(OverscanCalibratorTest, ClampMaxOverscan) {
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  CrosDisplayConfig* config = Shell::Get()->cros_display_config();

  // 1x display (800x600): max overscan per side is 200 horizontal, 150
  // vertical. Verify OverscanCalibration(kAdjust) clamps live preview insets.
  UpdateDisplay("800x600");
  int64_t display_id = display::Screen::Get()->GetPrimaryDisplay().id();
  OverscanCalibrator* calibrator = StartCalibration(display_id);
  ASSERT_TRUE(calibrator);

  EXPECT_EQ(
      DisplayConfigResult::kSuccess,
      config->OverscanCalibration(
          display_id, DisplayCalibrationOperation::kAdjust, gfx::Insets(500)));
  EXPECT_EQ(gfx::Insets::VH(150, 200), calibrator->insets());

  EXPECT_EQ(
      DisplayConfigResult::kSuccess,
      config->OverscanCalibration(
          display_id, DisplayCalibrationOperation::kAdjust, gfx::Insets(-500)));
  EXPECT_EQ(gfx::Insets(), calibrator->insets());

  EXPECT_EQ(DisplayConfigResult::kSuccess,
            config->OverscanCalibration(display_id,
                                        DisplayCalibrationOperation::kAdjust,
                                        gfx::Insets::TLBR(200, 300, 50, 60)));
  EXPECT_EQ(gfx::Insets::TLBR(150, 200, 50, 60), calibrator->insets());

  EXPECT_EQ(
      DisplayConfigResult::kSuccess,
      config->OverscanCalibration(
          display_id, DisplayCalibrationOperation::kComplete, std::nullopt));
  EXPECT_EQ(gfx::Insets::TLBR(150, 200, 50, 60),
            display_manager->GetOverscanInsets(display_id));
  EXPECT_EQ((gfx::Size{540, 400}),
            Shell::GetPrimaryRootWindow()->bounds().size());

  // Starting calibration with oversized stored insets clamps immediately, and
  // Reset() re-applies the clamped initial insets.
  display_manager->SetOverscanInsets(display_id, gfx::Insets(500));
  calibrator = StartCalibration(display_id);
  ASSERT_TRUE(calibrator);
  EXPECT_EQ(gfx::Insets::VH(150, 200), calibrator->insets());
  calibrator->UpdateInsets(gfx::Insets(10));
  EXPECT_EQ(gfx::Insets(10), calibrator->insets());
  EXPECT_EQ(DisplayConfigResult::kSuccess,
            config->OverscanCalibration(
                display_id, DisplayCalibrationOperation::kReset, std::nullopt));
  EXPECT_EQ(gfx::Insets::VH(150, 200), calibrator->insets());
  EXPECT_EQ(
      DisplayConfigResult::kSuccess,
      config->OverscanCalibration(
          display_id, DisplayCalibrationOperation::kComplete, std::nullopt));
  EXPECT_EQ(gfx::Insets::VH(150, 200),
            display_manager->GetOverscanInsets(display_id));
  EXPECT_EQ((gfx::Size{400, 300}),
            Shell::GetPrimaryRootWindow()->bounds().size());

  // Constructing OverscanCalibrator directly with oversized initial_insets also
  // clamps immediately and on Reset().
  {
    OverscanCalibrator direct_calibrator(
        display::Screen::Get()->GetPrimaryDisplay(), gfx::Insets(500));
    EXPECT_EQ(gfx::Insets::VH(150, 200), direct_calibrator.insets());
    direct_calibrator.UpdateInsets(gfx::Insets(10));
    EXPECT_EQ(gfx::Insets(10), direct_calibrator.insets());
    direct_calibrator.Reset();
    EXPECT_EQ(gfx::Insets::VH(150, 200), direct_calibrator.insets());
    direct_calibrator.Commit();
  }

  // 2x display (1920x1080*2): max overscan is 480x270 px = 240x135 DIP.
  UpdateDisplay("1920x1080*2");
  display_id = display::Screen::Get()->GetPrimaryDisplay().id();
  calibrator = StartCalibration(display_id);
  ASSERT_TRUE(calibrator);
  calibrator->UpdateInsets(gfx::Insets(500));
  EXPECT_EQ(gfx::Insets::VH(135, 240), calibrator->insets());
  calibrator->Commit();
  EXPECT_EQ(gfx::Insets::VH(135, 240),
            display_manager->GetOverscanInsets(display_id));
  EXPECT_EQ((gfx::Size{480, 270}),
            Shell::GetPrimaryRootWindow()->bounds().size());

  // Zoomed display (1200x800 @ 1.5x zoom): max pixel overscan is 300x200 px,
  // which is 200x133 in zoomed display DIPs and 300x199 in host DIPs.
  UpdateDisplay("1200x800");
  display_id = display::Screen::Get()->GetPrimaryDisplay().id();
  display_manager->UpdateZoomFactor(display_id, 1.5f);
  calibrator = StartCalibration(display_id);
  ASSERT_TRUE(calibrator);
  calibrator->UpdateInsets(gfx::Insets(500));
  EXPECT_EQ(gfx::Insets::VH(133, 200), calibrator->insets());
  calibrator->Commit();
  EXPECT_EQ(gfx::Insets::VH(199, 300),
            display_manager->GetOverscanInsets(display_id));
  EXPECT_EQ((gfx::Size{400, 268}),
            Shell::GetPrimaryRootWindow()->bounds().size());

  // Rotated 90-degree display (800x600/r -> 600x800 in display coordinates):
  // max in rotated display DIPs is 150 horizontal, 200 vertical.
  UpdateDisplay("800x600/r");
  display_id = display::Screen::Get()->GetPrimaryDisplay().id();
  calibrator = StartCalibration(display_id);
  ASSERT_TRUE(calibrator);
  calibrator->UpdateInsets(gfx::Insets(500));
  EXPECT_EQ(gfx::Insets::VH(200, 150), calibrator->insets());
  calibrator->Commit();
  EXPECT_EQ(gfx::Insets::VH(150, 200),
            display_manager->GetOverscanInsets(display_id));
  EXPECT_EQ((gfx::Size{300, 400}),
            Shell::GetPrimaryRootWindow()->bounds().size());
}

}  // namespace ash
