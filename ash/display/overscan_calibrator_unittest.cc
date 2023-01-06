// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/overscan_calibrator.h"
#include "ash/display/cros_display_config.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/stringprintf.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/managed_display_info.h"

namespace ash {

class OverscanCalibratorTest : public AshTestBase {
 public:
  OverscanCalibratorTest() = default;
  ~OverscanCalibratorTest() override = default;
  OverscanCalibratorTest(OverscanCalibratorTest&) = delete;
  OverscanCalibratorTest& operator=(const OverscanCalibratorTest&) = delete;

  OverscanCalibrator* StartCalibration(const std::string& id) {
    Shell::Get()->cros_display_config()->OverscanCalibration(
        id, crosapi::mojom::DisplayConfigOperation::kStart,
        gfx::Insets() /* not used */, base::DoNothing());
    return Shell::Get()->cros_display_config()->GetOverscanCalibrator(id);
  }
};

TEST_F(OverscanCalibratorTest, Rotation) {
  auto* display_manager = Shell::Get()->display_manager();

  int64_t display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  std::string id_str = base::StringPrintf("%" PRId64, display_id);

  auto* calibrator = StartCalibration(id_str);
  calibrator->UpdateInsets(gfx::Insets::TLBR(100, 5, 10, 15));
  calibrator->Commit();
  display::ManagedDisplayInfo info =
      display_manager->GetDisplayInfo(display_id);
  EXPECT_EQ(gfx::Insets::TLBR(100, 5, 10, 15), info.overscan_insets_in_dip());

  display_manager->SetDisplayRotation(display_id,
                                      display::Display::Rotation::ROTATE_90,
                                      display::Display::RotationSource::USER);
  EXPECT_EQ(gfx::Size(490, 780),
            display::Screen::GetScreen()->GetPrimaryDisplay().size());

  calibrator = StartCalibration(id_str);
  // The insets will be rotated and applied in the host coordinates.
  gfx::Insets insets = calibrator->insets();
  insets.set_left(105);
  insets.set_top(0);
  calibrator->UpdateInsets(insets);
  calibrator->Commit();

  info = display_manager->GetDisplayInfo(display_id);
  EXPECT_EQ(gfx::Insets::TLBR(105, 5, 10, 0), info.overscan_insets_in_dip());
}

}  // namespace ash
