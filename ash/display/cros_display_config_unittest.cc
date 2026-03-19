// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/cros_display_config.h"

#include <algorithm>

#include "ash/constants/ash_features.h"
#include "ash/display/display_alignment_controller.h"
#include "ash/display/display_highlight_controller.h"
#include "ash/display/screen_orientation_controller.h"
#include "ash/display/screen_orientation_controller_test_api.h"
#include "ash/display/touch_calibrator_controller.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/touch/ash_touch_transform_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/crosapi/mojom/cros_display_config.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "ui/display/display_switches.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/test/touch_transform_controller_test_api.h"
#include "ui/display/manager/touch_transform_setter.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/devices/touch_device_transform.h"
#include "ui/events/devices/touchscreen_device.h"

namespace ash {

namespace {

void SetResult(DisplayConfigResult* result_ptr,
               base::OnceClosure callback,
               DisplayConfigResult result) {
  *result_ptr = result;
  std::move(callback).Run();
}

void InitExternalTouchDevices(int64_t display_id) {
  ui::TouchscreenDevice touchdevice(123, ui::InputDeviceType::INPUT_DEVICE_USB,
                                    std::string("test external touch device"),
                                    gfx::Size(1000, 1000), 1);
  ui::DeviceDataManagerTestApi().SetTouchscreenDevices({touchdevice});

  std::vector<ui::TouchDeviceTransform> transforms;
  ui::TouchDeviceTransform touch_device_transform;
  touch_device_transform.display_id = display_id;
  touch_device_transform.device_id = touchdevice.id;
  transforms.push_back(touch_device_transform);
  display::test::TouchTransformControllerTestApi(
      Shell::Get()->touch_transformer_controller())
      .touch_transform_setter()
      ->ConfigureTouchDevices(transforms);
}

class TestObserver : public CrosDisplayConfig::Observer {
 public:
  TestObserver() = default;

  TestObserver(const TestObserver&) = delete;
  TestObserver& operator=(const TestObserver&) = delete;

  // CrosDisplayConfig::Observer:
  void OnDisplayConfigChanged() override { display_changes_++; }

  int display_changes() const { return display_changes_; }
  void reset_display_changes() { display_changes_ = 0; }

 private:
  int display_changes_ = 0;
};

}  // namespace

class CrosDisplayConfigTest : public AshTestBase {
 public:
  CrosDisplayConfigTest() = default;

  CrosDisplayConfigTest(const CrosDisplayConfigTest&) = delete;
  CrosDisplayConfigTest& operator=(const CrosDisplayConfigTest&) = delete;

  ~CrosDisplayConfigTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kDisplayAlignAssist);

    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kUseFirstDisplayAsInternal);
    AshTestBase::SetUp();
    CHECK(display::Screen::Get());
    cros_display_config_ = Shell::Get()->cros_display_config();
  }

  void TearDown() override {
    cros_display_config_ = nullptr;
    AshTestBase::TearDown();
  }

  DisplayLayoutInfo GetDisplayLayoutInfo() {
    return cros_display_config_->GetDisplayLayoutInfo();
  }

  DisplayConfigResult SetDisplayLayoutInfo(
      const DisplayLayoutInfo& display_layout_info) {
    return cros_display_config_->SetDisplayLayoutInfo(
        std::move(display_layout_info));
  }

  std::vector<DisplayUnitInfo> GetDisplayUnitInfoList(
      bool single_unified = false) {
    return cros_display_config_->GetDisplayUnitInfoList(single_unified);
  }

  DisplayConfigResult SetDisplayProperties(
      const std::string& id,
      const DisplayConfigProperties& properties) {
    return cros_display_config_->SetDisplayProperties(
        id, properties, crosapi::mojom::DisplayConfigSource::kUser);
  }

  bool OverscanCalibration(int64_t id,
                           crosapi::mojom::DisplayConfigOperation op,
                           const std::optional<gfx::Insets>& delta) {
    return cros_display_config()->OverscanCalibration(base::NumberToString(id),
                                                      op, delta) ==
           DisplayConfigResult::kSuccess;
  }

  bool DisplayExists(int64_t display_id) {
    const display::Display& display =
        display_manager()->GetDisplayForId(display_id);
    return display.id() != display::kInvalidDisplayId;
  }

  bool StartTouchCalibration(const std::string& display_id) {
    return CallTouchCalibration(display_id,
                                crosapi::mojom::DisplayConfigOperation::kStart,
                                std::nullopt);
  }

  bool CompleteCustomTouchCalibration(
      const std::string& display_id,
      const display::TouchCalibrationData& calibration) {
    return CallTouchCalibration(
        display_id, crosapi::mojom::DisplayConfigOperation::kComplete,
        calibration);
  }

  bool CallTouchCalibration(
      const std::string& id,
      crosapi::mojom::DisplayConfigOperation op,
      base::optional_ref<const display::TouchCalibrationData> calibration) {
    DisplayConfigResult result;
    base::RunLoop run_loop;
    cros_display_config_->TouchCalibration(
        id, op, calibration,
        base::BindOnce(&SetResult, &result, run_loop.QuitClosure()));
    run_loop.Run();
    return result == DisplayConfigResult::kSuccess;
  }

  bool IsTouchCalibrationActive() {
    TouchCalibratorController* touch_calibrator =
        cros_display_config_->touch_calibrator_for_test();
    return touch_calibrator && touch_calibrator->IsCalibrating();
  }

  void HighlightDisplay(int64_t id) {
    cros_display_config_->HighlightDisplay(id);
  }

  void DragDisplayDelta(int64_t id, int32_t delta_x, int32_t delta_y) {
    cros_display_config_->DragDisplayDelta(id, delta_x, delta_y);
  }

  bool PreviewIndicatorsExist() {
    return !Shell::Get()
                ->display_alignment_controller()
                ->GetActiveIndicatorsForTesting()
                .empty();
  }

  CrosDisplayConfigImpl* cros_display_config() { return cros_display_config_; }

 private:
  raw_ptr<CrosDisplayConfigImpl> cros_display_config_ = nullptr;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(CrosDisplayConfigTest, OnDisplayConfigChanged) {
  TestObserver observer;
  cros_display_config()->AddObserver(&observer);

  // Adding one display should trigger one notification.
  UpdateDisplay("500x400");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, observer.display_changes());
  observer.reset_display_changes();

  // Adding one display should trigger just one notification.
  UpdateDisplay("500x400,500x400");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, observer.display_changes());

  cros_display_config()->RemoveObserver(&observer);
}

TEST_F(CrosDisplayConfigTest, GetDisplayLayoutInfo) {
  UpdateDisplay("500x400,500x400,500x400");
  std::vector<display::Display> displays =
      display::Screen::Get()->GetAllDisplays();
  ASSERT_EQ(3u, displays.size());

  DisplayLayoutInfo display_layout_info = GetDisplayLayoutInfo();
  const std::vector<display::DisplayPlacement>& layouts =
      *display_layout_info.layouts;
  ASSERT_EQ(2u, layouts.size());

  EXPECT_EQ(displays[1].id(), layouts[0].display_id);
  EXPECT_EQ(displays[0].id(), layouts[0].parent_display_id);
  EXPECT_EQ(display::DisplayPlacement::RIGHT, layouts[0].position);
  EXPECT_EQ(0, layouts[0].offset);

  EXPECT_EQ(displays[2].id(), layouts[1].display_id);
  EXPECT_EQ(layouts[0].display_id, layouts[1].parent_display_id);
  EXPECT_EQ(display::DisplayPlacement::RIGHT, layouts[1].position);
  EXPECT_EQ(0, layouts[1].offset);
}

TEST_F(CrosDisplayConfigTest, FailToSetLayoutUnifiedWithOneDisplay) {
  UpdateDisplay("500x400");
  EXPECT_FALSE(display_manager()->IsInUnifiedMode());

  // Enable unified desktop and expect to fail due to not enough connected
  // displays.
  DisplayLayoutInfo properties;
  properties.layout_mode = DisplayLayoutMode::kUnified;
  DisplayConfigResult result = SetDisplayLayoutInfo(std::move(properties));
  EXPECT_EQ(DisplayConfigResult::kSingleDisplayError, result);
  EXPECT_FALSE(display_manager()->IsInUnifiedMode());
}

TEST_F(CrosDisplayConfigTest, SetLayoutUnified) {
  UpdateDisplay("500x400,500x400");
  EXPECT_FALSE(display_manager()->IsInUnifiedMode());

  // Enable unified desktop. Enables unified mode.
  cros_display_config()->SetUnifiedDesktopEnabled(true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(display_manager()->IsInUnifiedMode());

  // Disable unified mode.
  {
    DisplayLayoutInfo properties;
    properties.layout_mode = DisplayLayoutMode::kNormal;
    DisplayConfigResult result = SetDisplayLayoutInfo(std::move(properties));
    EXPECT_EQ(DisplayConfigResult::kSuccess, result);
    EXPECT_FALSE(display_manager()->IsInUnifiedMode());
  }

  // Enable unified mode.
  {
    DisplayLayoutInfo properties;
    properties.layout_mode = DisplayLayoutMode::kUnified;
    DisplayConfigResult result = SetDisplayLayoutInfo(std::move(properties));
    EXPECT_EQ(DisplayConfigResult::kSuccess, result);
    EXPECT_TRUE(display_manager()->IsInUnifiedMode());
  }

  // Restore extended mode.
  cros_display_config()->SetUnifiedDesktopEnabled(false);
  EXPECT_FALSE(display_manager()->IsInUnifiedMode());
}

// Make sure that available zoom factors can be correctly set
// in unified desktoip mode.
TEST_F(CrosDisplayConfigTest, SetLayoutUnifiedWithZoomFactors) {
  UpdateDisplay("1920x1080/r,1920x1080,1920x1080");
  EXPECT_FALSE(display_manager()->IsInUnifiedMode());

  // Enable unified desktop. Enables unified mode.
  cros_display_config()->SetUnifiedDesktopEnabled(true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(display_manager()->IsInUnifiedMode());

  auto display_unit_info_ptr_list =
      GetDisplayUnitInfoList(/*single_unified=*/true);

  ASSERT_EQ(1u, display_unit_info_ptr_list.size());
  auto zoom_factors =
      display_unit_info_ptr_list[0].available_display_zoom_factors;

  auto primary_id = display::Screen::Get()->GetPrimaryDisplay().id();

  for (auto zoom_factor : zoom_factors) {
    DisplayConfigProperties properties;
    properties.display_zoom_factor = zoom_factor;
    DisplayConfigResult result =
        SetDisplayProperties(base::NumberToString(primary_id), properties);
    EXPECT_EQ(DisplayConfigResult::kSuccess, result);
    EXPECT_TRUE(display_manager()->IsInUnifiedMode());
    EXPECT_EQ(zoom_factor,
              display_manager()->GetDisplayInfo(primary_id).zoom_factor());
  }
}

TEST_F(CrosDisplayConfigTest, FailToSetLayoutMirroredDefaultWithOneDisplay) {
  UpdateDisplay("500x400");
  EXPECT_FALSE(display_manager()->IsInMirrorMode());

  // Enable default mirror mode and expect to fail due to not enough connected
  // displays.
  DisplayLayoutInfo properties;
  properties.layout_mode = DisplayLayoutMode::kMirrored;
  DisplayConfigResult result = SetDisplayLayoutInfo(std::move(properties));
  EXPECT_EQ(DisplayConfigResult::kSingleDisplayError, result);
  EXPECT_FALSE(display_manager()->IsInMirrorMode());

  display::DisplayIdList id_list =
      display_manager()->GetMirroringDestinationDisplayIdList();
  ASSERT_TRUE(id_list.empty());
}

TEST_F(CrosDisplayConfigTest, SetLayoutMirroredDefault) {
  UpdateDisplay("500x400,500x400,500x400");

  {
    DisplayLayoutInfo properties;
    properties.layout_mode = DisplayLayoutMode::kMirrored;
    DisplayConfigResult result = SetDisplayLayoutInfo(std::move(properties));
    EXPECT_EQ(DisplayConfigResult::kSuccess, result);
    EXPECT_TRUE(display_manager()->IsInMirrorMode());
    display::DisplayIdList id_list =
        display_manager()->GetMirroringDestinationDisplayIdList();
    ASSERT_EQ(2u, id_list.size());
  }

  {
    DisplayLayoutInfo properties;
    properties.layout_mode = DisplayLayoutMode::kNormal;
    DisplayConfigResult result = SetDisplayLayoutInfo(std::move(properties));
    EXPECT_EQ(DisplayConfigResult::kSuccess, result);
    EXPECT_FALSE(display_manager()->IsInMirrorMode());
  }
}

TEST_F(CrosDisplayConfigTest, FailToSetLayoutMirroredMixedWithOneDisplay) {
  UpdateDisplay("500x400");
  EXPECT_FALSE(display_manager()->IsInMirrorMode());

  std::vector<display::Display> displays =
      display::Screen::Get()->GetAllDisplays();
  ASSERT_EQ(1u, displays.size());

  // Enable mixed mirror mode and expect to fail due to not enough connected
  // displays.
  DisplayLayoutInfo properties;
  properties.layout_mode = DisplayLayoutMode::kMirrored;
  properties.mirror_source_id = displays[0].id();
  properties.mirror_destination_ids.emplace();

  DisplayConfigResult result = SetDisplayLayoutInfo(std::move(properties));
  EXPECT_EQ(DisplayConfigResult::kSingleDisplayError, result);
  EXPECT_FALSE(display_manager()->IsInMirrorMode());

  display::DisplayIdList id_list =
      display_manager()->GetMirroringDestinationDisplayIdList();
  ASSERT_TRUE(id_list.empty());
}

TEST_F(CrosDisplayConfigTest, SetLayoutMirroredMixed) {
  UpdateDisplay("500x400,500x400,500x400,500x400");

  std::vector<display::Display> displays =
      display::Screen::Get()->GetAllDisplays();
  ASSERT_EQ(4u, displays.size());

  DisplayLayoutInfo properties;
  properties.layout_mode = DisplayLayoutMode::kMirrored;
  properties.mirror_source_id = displays[0].id();
  properties.mirror_destination_ids.emplace(
      {displays[1].id(), displays[3].id()});
  DisplayConfigResult result = SetDisplayLayoutInfo(std::move(properties));
  EXPECT_EQ(DisplayConfigResult::kSuccess, result);
  EXPECT_TRUE(display_manager()->IsInMirrorMode());
  display::DisplayIdList id_list =
      display_manager()->GetMirroringDestinationDisplayIdList();
  ASSERT_EQ(2u, id_list.size());
  EXPECT_TRUE(std::ranges::contains(id_list, displays[1].id()));
  EXPECT_TRUE(std::ranges::contains(id_list, displays[3].id()));
}

TEST_F(CrosDisplayConfigTest, GetDisplayUnitInfoListBasic) {
  UpdateDisplay("500x600,400x520");
  std::vector<DisplayUnitInfo> result = GetDisplayUnitInfoList();
  ASSERT_EQ(2u, result.size());

  ASSERT_TRUE(DisplayExists(result[0].id));
  EXPECT_TRUE(result[0].is_primary);
  EXPECT_TRUE(result[0].is_internal);
  EXPECT_TRUE(result[0].is_enabled);
  EXPECT_FALSE(result[0].is_auto_rotation_allowed);
  EXPECT_FALSE(result[0].has_touch_support);
  EXPECT_FALSE(result[0].has_accelerometer_support);
  EXPECT_EQ(96, result[0].dpi_x);
  EXPECT_EQ(96, result[0].dpi_y);
  EXPECT_EQ(crosapi::mojom::DisplayRotationOptions::kZeroDegrees,
            result[0].rotation_options);
  EXPECT_EQ(gfx::Rect(0, 0, 500, 600), result[0].bounds);
  EXPECT_EQ(gfx::Insets(), result[0].overscan);

  ASSERT_TRUE(DisplayExists(result[1].id));
  EXPECT_EQ(display_manager()->GetDisplayNameForId(result[1].id),
            result[1].name);
  // Second display is left of the primary display whose width 500.
  EXPECT_EQ(gfx::Rect(500, 0, 400, 520), result[1].bounds);
  EXPECT_EQ(gfx::Insets(), result[1].overscan);
  EXPECT_EQ(crosapi::mojom::DisplayRotationOptions::kZeroDegrees,
            result[1].rotation_options);
  EXPECT_FALSE(result[1].is_primary);
  EXPECT_FALSE(result[1].is_internal);
  EXPECT_TRUE(result[1].is_enabled);
  EXPECT_EQ(96, result[1].dpi_x);
  EXPECT_EQ(96, result[1].dpi_y);
}

TEST_F(CrosDisplayConfigTest, GetDisplayUnitInfoListZoomFactor) {
  UpdateDisplay("1024x512,1024x512");
  std::vector<DisplayUnitInfo> result = GetDisplayUnitInfoList();
  ASSERT_EQ(2u, result.size());

  EXPECT_EQ(1.0, result[0].display_zoom_factor);
  const std::vector<double>& zoom_factors =
      result[0].available_display_zoom_factors;
  EXPECT_EQ(9u, zoom_factors.size());
  EXPECT_FLOAT_EQ(0.90f, zoom_factors[0]);
  EXPECT_FLOAT_EQ(0.95f, zoom_factors[1]);
  EXPECT_FLOAT_EQ(1.f, zoom_factors[2]);
  EXPECT_FLOAT_EQ(1.05f, zoom_factors[3]);
  EXPECT_FLOAT_EQ(1.10f, zoom_factors[4]);
  EXPECT_FLOAT_EQ(1.15f, zoom_factors[5]);
  EXPECT_FLOAT_EQ(1.20f, zoom_factors[6]);
  EXPECT_FLOAT_EQ(1.25f, zoom_factors[7]);
  EXPECT_FLOAT_EQ(1.30f, zoom_factors[8]);
}

TEST_F(CrosDisplayConfigTest, SetDisplayPropertiesPrimary) {
  UpdateDisplay("1200x600,600x1000");
  int64_t primary_id = display::Screen::Get()->GetPrimaryDisplay().id();
  int64_t secondary_id = display::test::DisplayManagerTestApi(display_manager())
                             .GetSecondaryDisplay()
                             .id();
  ASSERT_NE(primary_id, secondary_id);

  DisplayConfigProperties properties;
  properties.set_primary = true;
  DisplayConfigResult result =
      SetDisplayProperties(base::NumberToString(secondary_id), properties);
  EXPECT_EQ(DisplayConfigResult::kSuccess, result);

  // secondary display should now be primary.
  primary_id = display::Screen::Get()->GetPrimaryDisplay().id();
  EXPECT_EQ(primary_id, secondary_id);
}

TEST_F(CrosDisplayConfigTest, SetDisplayPropertiesOverscan) {
  UpdateDisplay("1200x600,600x1000*2");
  const display::Display& secondary =
      display::test::DisplayManagerTestApi(display_manager())
          .GetSecondaryDisplay();

  DisplayConfigProperties properties;
  properties.overscan = gfx::Insets::TLBR(199, 20, 51, 130);
  DisplayConfigResult result =
      SetDisplayProperties(base::NumberToString(secondary.id()), properties);
  EXPECT_EQ(DisplayConfigResult::kSuccess, result);
  EXPECT_EQ(gfx::Rect(1200, 0, 150, 250), secondary.bounds());
  const gfx::Insets overscan =
      display_manager()->GetOverscanInsets(secondary.id());
  EXPECT_EQ(199, overscan.top());
  EXPECT_EQ(20, overscan.left());
  EXPECT_EQ(51, overscan.bottom());
  EXPECT_EQ(130, overscan.right());
}

TEST_F(CrosDisplayConfigTest, SetDisplayPropertiesRotation) {
  UpdateDisplay("1200x600,600x1000*2");
  const display::Display& secondary =
      display::test::DisplayManagerTestApi(display_manager())
          .GetSecondaryDisplay();

  {
    DisplayConfigProperties properties;
    properties.rotation = crosapi::mojom::DisplayRotationOptions::k90Degrees;
    auto result =
        SetDisplayProperties(base::NumberToString(secondary.id()), properties);
    EXPECT_EQ(DisplayConfigResult::kSuccess, result);
    EXPECT_EQ(gfx::Rect(1200, 0, 500, 300), secondary.bounds());
    EXPECT_EQ(display::Display::ROTATE_90, secondary.rotation());
  }

  {
    DisplayConfigProperties properties;
    properties.rotation = crosapi::mojom::DisplayRotationOptions::k270Degrees;
    auto result =
        SetDisplayProperties(base::NumberToString(secondary.id()), properties);
    EXPECT_EQ(DisplayConfigResult::kSuccess, result);
    EXPECT_EQ(gfx::Rect(1200, 0, 500, 300), secondary.bounds());
    EXPECT_EQ(display::Display::ROTATE_270, secondary.rotation());
  }

  // Test setting primary and rotating.
  {
    DisplayConfigProperties properties;
    properties.set_primary = true;
    properties.rotation = crosapi::mojom::DisplayRotationOptions::k180Degrees;
    auto result =
        SetDisplayProperties(base::NumberToString(secondary.id()), properties);
    EXPECT_EQ(DisplayConfigResult::kSuccess, result);
    const display::Display& primary =
        display::Screen::Get()->GetPrimaryDisplay();
    EXPECT_EQ(secondary.id(), primary.id());
    EXPECT_EQ(gfx::Rect(0, 0, 300, 500), primary.bounds());
    EXPECT_EQ(display::Display::ROTATE_180, primary.rotation());
  }
}

TEST_F(CrosDisplayConfigTest, SetDisplayPropertiesBoundsOrigin) {
  UpdateDisplay("1200x600,520x400");
  const display::Display& secondary =
      display::test::DisplayManagerTestApi(display_manager())
          .GetSecondaryDisplay();

  {
    DisplayConfigProperties properties;
    properties.bounds_origin = gfx::Point({-520, 50});
    auto result =
        SetDisplayProperties(base::NumberToString(secondary.id()), properties);
    EXPECT_EQ(DisplayConfigResult::kSuccess, result);
    EXPECT_EQ(gfx::Rect(-520, 50, 520, 400), secondary.bounds());
  }

  {
    DisplayConfigProperties properties;
    properties.bounds_origin = gfx::Point({1200, 100});
    auto result =
        SetDisplayProperties(base::NumberToString(secondary.id()), properties);
    EXPECT_EQ(DisplayConfigResult::kSuccess, result);
    EXPECT_EQ(gfx::Rect(1200, 100, 520, 400), secondary.bounds());
  }

  {
    DisplayConfigProperties properties;
    properties.bounds_origin = gfx::Point({1100, -400});
    auto result =
        SetDisplayProperties(base::NumberToString(secondary.id()), properties);
    EXPECT_EQ(DisplayConfigResult::kSuccess, result);
    EXPECT_EQ(gfx::Rect(1100, -400, 520, 400), secondary.bounds());
  }

  {
    DisplayConfigProperties properties;
    properties.bounds_origin = gfx::Point({-350, 600});
    auto result =
        SetDisplayProperties(base::NumberToString(secondary.id()), properties);
    EXPECT_EQ(DisplayConfigResult::kSuccess, result);
    EXPECT_EQ(gfx::Rect(-350, 600, 520, 400), secondary.bounds());
  }
}

TEST_F(CrosDisplayConfigTest, SetDisplayPropertiesDisplayZoomFactor) {
  static std::string configs[] = {
      "1200x600, 1600x1000#1600x1000",  // landscape
      "600x1200, 1000x1600#1000x1600",  // portrait
  };
  for (auto config : configs) {
    SCOPED_TRACE(config);
    UpdateDisplay(config);
    display::DisplayIdList display_id_list =
        display_manager()->GetConnectedDisplayIdList();

    const float zoom_factor_1 = 1.23f;
    const float zoom_factor_2 = 2.34f;

    display_manager()->UpdateZoomFactor(display_id_list[0], zoom_factor_2);
    display_manager()->UpdateZoomFactor(display_id_list[1], zoom_factor_1);

    EXPECT_EQ(
        zoom_factor_2,
        display_manager()->GetDisplayInfo(display_id_list[0]).zoom_factor());
    EXPECT_EQ(
        zoom_factor_1,
        display_manager()->GetDisplayInfo(display_id_list[1]).zoom_factor());

    // Set zoom factor for display 0, should not affect display 1.
    {
      DisplayConfigProperties properties;
      properties.display_zoom_factor = zoom_factor_1;
      auto result = SetDisplayProperties(
          base::NumberToString(display_id_list[0]), properties);
      EXPECT_EQ(DisplayConfigResult::kSuccess, result);
      EXPECT_EQ(
          zoom_factor_1,
          display_manager()->GetDisplayInfo(display_id_list[0]).zoom_factor());
      EXPECT_EQ(
          zoom_factor_1,
          display_manager()->GetDisplayInfo(display_id_list[1]).zoom_factor());
    }

    // Set zoom factor for display 1.
    {
      DisplayConfigProperties properties;
      properties.display_zoom_factor = zoom_factor_2;
      auto result = SetDisplayProperties(
          base::NumberToString(display_id_list[1]), properties);
      EXPECT_EQ(DisplayConfigResult::kSuccess, result);
      EXPECT_EQ(
          zoom_factor_1,
          display_manager()->GetDisplayInfo(display_id_list[0]).zoom_factor());
      EXPECT_EQ(
          zoom_factor_2,
          display_manager()->GetDisplayInfo(display_id_list[1]).zoom_factor());
    }

    // Invalid zoom factor should fail.
    {
      const float invalid_zoom_factor = 0.01f;
      DisplayConfigProperties properties;
      properties.display_zoom_factor = invalid_zoom_factor;
      auto result = SetDisplayProperties(
          base::NumberToString(display_id_list[1]), properties);
      EXPECT_EQ(DisplayConfigResult::kPropertyValueOutOfRangeError, result);
      EXPECT_EQ(
          zoom_factor_2,
          display_manager()->GetDisplayInfo(display_id_list[1]).zoom_factor());
    }
  }
}

TEST_F(CrosDisplayConfigTest, SetDisplayMode) {
  UpdateDisplay("1024x512,1024x512");
  std::vector<DisplayUnitInfo> result = GetDisplayUnitInfoList();
  ASSERT_EQ(2u, result.size());
  // Internal display has just one mode.
  EXPECT_EQ(0, result[0].selected_display_mode_index);
  ASSERT_EQ(1u, result[0].available_display_modes.size());

  DisplayConfigProperties properties;
  properties.display_mode = result[0].available_display_modes[0];
  ASSERT_EQ(
      DisplayConfigResult::kSuccess,
      SetDisplayProperties(base::NumberToString(result[0].id), properties));

  result = GetDisplayUnitInfoList();
  ASSERT_EQ(2u, result.size());
  EXPECT_EQ(0, result[0].selected_display_mode_index);
}

TEST_F(CrosDisplayConfigTest, OverscanCalibration) {
  UpdateDisplay("1200x600");
  int64_t id = display::Screen::Get()->GetPrimaryDisplay().id();
  ASSERT_NE(display::kInvalidDisplayId, id);

  // Test that kAdjust succeeds after kComplete call.
  EXPECT_TRUE(OverscanCalibration(
      id, crosapi::mojom::DisplayConfigOperation::kStart, std::nullopt));
  EXPECT_EQ(gfx::Insets(), display_manager()->GetOverscanInsets(id));

  gfx::Insets insets(10);
  EXPECT_TRUE(OverscanCalibration(
      id, crosapi::mojom::DisplayConfigOperation::kAdjust, insets));
  // Adjust has no effect until Complete.
  EXPECT_EQ(gfx::Insets(), display_manager()->GetOverscanInsets(id));

  EXPECT_TRUE(OverscanCalibration(
      id, crosapi::mojom::DisplayConfigOperation::kComplete, std::nullopt));
  gfx::Insets overscan = display_manager()->GetOverscanInsets(id);
  EXPECT_EQ(insets, overscan)
      << "Overscan: " << overscan.ToString() << " != " << insets.ToString();

  // Test that kReset clears restores previous insets.

  // Start clears any overscan values.
  EXPECT_TRUE(OverscanCalibration(
      id, crosapi::mojom::DisplayConfigOperation::kStart, std::nullopt));
  EXPECT_EQ(gfx::Insets(), display_manager()->GetOverscanInsets(id));

  // Reset + Complete restores previously set insets.
  EXPECT_TRUE(OverscanCalibration(
      id, crosapi::mojom::DisplayConfigOperation::kReset, std::nullopt));
  EXPECT_EQ(gfx::Insets(), display_manager()->GetOverscanInsets(id));
  EXPECT_TRUE(OverscanCalibration(
      id, crosapi::mojom::DisplayConfigOperation::kComplete, std::nullopt));
  EXPECT_EQ(insets, display_manager()->GetOverscanInsets(id));

  // Additional complete call should fail.
  EXPECT_FALSE(OverscanCalibration(
      id, crosapi::mojom::DisplayConfigOperation::kComplete, std::nullopt));
}

TEST_F(CrosDisplayConfigTest, CustomTouchCalibrationInternal) {
  UpdateDisplay("1200x600,600x1000*2");
  const int64_t internal_display_id =
      display::test::DisplayManagerTestApi(display_manager())
          .SetFirstDisplayAsInternalDisplay();

  InitExternalTouchDevices(internal_display_id);

  EXPECT_FALSE(
      StartTouchCalibration(base::NumberToString(internal_display_id)));
  EXPECT_FALSE(IsTouchCalibrationActive());
}

TEST_F(CrosDisplayConfigTest, CustomTouchCalibrationWithoutStart) {
  UpdateDisplay("1200x600,600x1000*2");
  EXPECT_FALSE(IsTouchCalibrationActive());
}

TEST_F(CrosDisplayConfigTest, CustomTouchCalibrationNonTouchDisplay) {
  UpdateDisplay("1200x600,600x1000*2");

  const int64_t internal_display_id =
      display::test::DisplayManagerTestApi(display_manager())
          .SetFirstDisplayAsInternalDisplay();

  display::DisplayIdList display_id_list =
      display_manager()->GetConnectedDisplayIdList();

  // Pick the non internal display Id.
  const int64_t display_id = display_id_list[0] == internal_display_id
                                 ? display_id_list[1]
                                 : display_id_list[0];

  ui::DeviceDataManagerTestApi().SetTouchscreenDevices({});
  std::string id = base::NumberToString(display_id);

  // Since no external touch devices are present, the calibration should fail.
  EXPECT_FALSE(StartTouchCalibration(id));

  // If an external touch device is present, the calibration should proceed.
  InitExternalTouchDevices(display_id);
  EXPECT_TRUE(StartTouchCalibration(id));
  EXPECT_TRUE(IsTouchCalibrationActive());
}

TEST_F(CrosDisplayConfigTest, CustomTouchCalibrationInvalidPoints) {
  UpdateDisplay("1200x600,600x1000*2");

  const int64_t internal_display_id =
      display::test::DisplayManagerTestApi(display_manager())
          .SetFirstDisplayAsInternalDisplay();

  display::DisplayIdList display_id_list =
      display_manager()->GetConnectedDisplayIdList();

  // Pick the non internal display Id.
  const int64_t display_id = display_id_list[0] == internal_display_id
                                 ? display_id_list[1]
                                 : display_id_list[0];

  InitExternalTouchDevices(display_id);

  std::string id = base::NumberToString(display_id);

  {
    EXPECT_TRUE(StartTouchCalibration(id));
    display::TouchCalibrationData calibration;
    calibration.point_pairs[0].first.set_x(-1);
    EXPECT_FALSE(CompleteCustomTouchCalibration(id, calibration));
  }

  {
    EXPECT_TRUE(StartTouchCalibration(id));
    display::TouchCalibrationData calibration;
    calibration.bounds.set_width(1);
    calibration.point_pairs[0].first.set_x(2);
    EXPECT_FALSE(CompleteCustomTouchCalibration(id, calibration));
  }
}

TEST_F(CrosDisplayConfigTest, CustomTouchCalibrationSuccess) {
  UpdateDisplay("1200x600,600x1000*2");

  const int64_t internal_display_id =
      display::test::DisplayManagerTestApi(display_manager())
          .SetFirstDisplayAsInternalDisplay();

  display::DisplayIdList display_id_list =
      display_manager()->GetConnectedDisplayIdList();

  // Pick the non internal display Id.
  const int64_t display_id = display_id_list[0] == internal_display_id
                                 ? display_id_list[1]
                                 : display_id_list[0];

  InitExternalTouchDevices(display_id);

  std::string id = base::NumberToString(display_id);

  EXPECT_TRUE(StartTouchCalibration(id));
  EXPECT_TRUE(IsTouchCalibrationActive());
  display::TouchCalibrationData calibration;
  EXPECT_TRUE(CompleteCustomTouchCalibration(id, calibration));
}

TEST_F(CrosDisplayConfigTest, TabletModeAutoRotationInternalOnly) {
  UpdateDisplay("500x600,400x520");

  auto* screen_orientation_controller =
      Shell::Get()->screen_orientation_controller();
  EXPECT_FALSE(screen_orientation_controller->user_rotation_locked());

  TabletModeControllerTestApi tablet_mode_controller_test_api;
  ScreenOrientationControllerTestApi screen_orientation_controller_test_api(
      screen_orientation_controller);
  tablet_mode_controller_test_api.EnterTabletMode();
  EXPECT_TRUE(tablet_mode_controller_test_api.IsInPhysicalTabletState());
  EXPECT_TRUE(screen_orientation_controller_test_api.IsAutoRotationAllowed());
  EXPECT_TRUE(display::Screen::Get()->InTabletMode());

  std::vector<DisplayUnitInfo> result = GetDisplayUnitInfoList();
  ASSERT_EQ(2u, result.size());

  ASSERT_TRUE(DisplayExists(result[0].id));
  EXPECT_TRUE(result[0].is_internal);
  EXPECT_TRUE(result[0].is_auto_rotation_allowed);

  ASSERT_TRUE(DisplayExists(result[1].id));
  EXPECT_FALSE(result[1].is_internal);
  EXPECT_FALSE(result[1].is_auto_rotation_allowed);
}

TEST_F(CrosDisplayConfigTest, TabletModeAutoRotation) {
  TestObserver observer;
  cros_display_config()->AddObserver(&observer);

  display::test::DisplayManagerTestApi(display_manager())
      .SetFirstDisplayAsInternalDisplay();
  const display::Display& display =
      display_manager()->GetPrimaryDisplayCandidate();
  auto* screen_orientation_controller =
      Shell::Get()->screen_orientation_controller();
  TabletModeControllerTestApi tablet_mode_controller_test_api;
  ScreenOrientationControllerTestApi screen_orientation_controller_test_api(
      screen_orientation_controller);

  // Setting the rotation to kAutoRotate from outside the physical tablet state
  // is treated as a request to set the rotation to 0.
  {
    DisplayConfigProperties properties;
    properties.rotation = crosapi::mojom::DisplayRotationOptions::kAutoRotate;
    auto result =
        SetDisplayProperties(base::NumberToString(display.id()), properties);
    EXPECT_EQ(DisplayConfigResult::kSuccess, result);
    EXPECT_FALSE(screen_orientation_controller->user_rotation_locked());
    EXPECT_EQ(display::Display::ROTATE_0, display.rotation());

    tablet_mode_controller_test_api.EnterTabletMode();
    EXPECT_TRUE(tablet_mode_controller_test_api.IsInPhysicalTabletState());
    EXPECT_TRUE(screen_orientation_controller_test_api.IsAutoRotationAllowed());
    EXPECT_TRUE(display::Screen::Get()->InTabletMode());

    // Clear out any pending observer calls.
    base::RunLoop().RunUntilIdle();
    observer.reset_display_changes();
  }

  {
    DisplayConfigProperties properties;
    properties.rotation = crosapi::mojom::DisplayRotationOptions::k90Degrees;
    auto result =
        SetDisplayProperties(base::NumberToString(display.id()), properties);
    EXPECT_EQ(DisplayConfigResult::kSuccess, result);
    EXPECT_TRUE(screen_orientation_controller->user_rotation_locked());
    EXPECT_EQ(display::Display::ROTATE_90, display.rotation());
    // OnDisplayConfigChanged() will be called twice, once as a result of the
    // user rotation lock change, and another due to the actual display rotation
    // change.
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(2, observer.display_changes());

    // Hooking up an external mouse device, should exit UI tablet mode, but the
    // device is still in a tablet physical state, the API should still be valid
    // for use.
    tablet_mode_controller_test_api.AttachExternalMouse();
    EXPECT_TRUE(tablet_mode_controller_test_api.IsInPhysicalTabletState());
    EXPECT_TRUE(screen_orientation_controller_test_api.IsAutoRotationAllowed());
    EXPECT_FALSE(display::Screen::Get()->InTabletMode());

    // Clear out any pending observer calls.
    base::RunLoop().RunUntilIdle();
    observer.reset_display_changes();
  }

  {
    DisplayConfigProperties properties;
    properties.rotation = crosapi::mojom::DisplayRotationOptions::kAutoRotate;
    auto result =
        SetDisplayProperties(base::NumberToString(display.id()), properties);
    EXPECT_EQ(DisplayConfigResult::kSuccess, result);
    EXPECT_FALSE(screen_orientation_controller->user_rotation_locked());
    // Unlocking auto-rotate doesn't actually change the display rotation. It
    // simply allows it to auto-rotate in response to accelerometer updates.
    EXPECT_EQ(display::Display::ROTATE_90, display.rotation());
    // This time, OnDisplayConfigChanged() will be called only once as a result
    // of the user rotation lock change.
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(1, observer.display_changes());

    // Once the device is no longer in a physical tablet state, the rotation is
    // restored.
    tablet_mode_controller_test_api.LeaveTabletMode();
    EXPECT_FALSE(tablet_mode_controller_test_api.IsInPhysicalTabletState());
    EXPECT_FALSE(
        screen_orientation_controller_test_api.IsAutoRotationAllowed());
    EXPECT_FALSE(display::Screen::Get()->InTabletMode());
    EXPECT_EQ(display::Display::ROTATE_0, display.rotation());

    cros_display_config()->RemoveObserver(&observer);
  }
}

TEST_F(CrosDisplayConfigTest, HighlightDisplayValid) {
  UpdateDisplay("500x400,500x400");

  const display::Display& display = display_manager()->GetDisplayAt(0);
  const int64_t display_id = display.id();

  HighlightDisplay(display_id);

  views::Widget* widget =
      Shell::Get()->display_highlight_controller()->GetWidgetForTesting();
  ASSERT_NE(widget, nullptr);
  EXPECT_EQ(widget->GetNativeWindow()->GetRootWindow(),
            Shell::GetRootWindowForDisplayId(display_id));
}

TEST_F(CrosDisplayConfigTest, HighlightDisplayInvalid) {
  UpdateDisplay("500x400,500x400");

  HighlightDisplay(display::kInvalidDisplayId);

  EXPECT_EQ(Shell::Get()->display_highlight_controller()->GetWidgetForTesting(),
            nullptr);
}

TEST_F(CrosDisplayConfigTest, DragDisplayDelta) {
  UpdateDisplay("500x400,500x400");

  const auto& display = display_manager()->GetDisplayAt(0);

  EXPECT_FALSE(PreviewIndicatorsExist());

  DragDisplayDelta(display.id(), 0, 16);

  EXPECT_TRUE(PreviewIndicatorsExist());
}

}  // namespace ash
