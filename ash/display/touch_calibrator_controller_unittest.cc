// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/touch_calibrator_controller.h"

#include <vector>

#include "ash/display/touch_calibrator_view.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/touch/ash_touch_transform_controller.h"
#include "base/stl_util.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/test/touch_device_manager_test_api.h"
#include "ui/display/manager/test/touch_transform_controller_test_api.h"
#include "ui/display/manager/touch_device_manager.h"
#include "ui/display/manager/touch_transform_setter.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/devices/touch_device_transform.h"
#include "ui/events/event_handler.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/test/events_test_utils.h"

namespace ash {
namespace {

ui::TouchscreenDevice GetInternalTouchDevice(int touch_device_id) {
  return ui::TouchscreenDevice(
      touch_device_id, ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
      std::string("test internal touch device"), gfx::Size(1000, 1000), 1);
}

ui::TouchscreenDevice GetExternalTouchDevice(int touch_device_id) {
  return ui::TouchscreenDevice(
      touch_device_id, ui::InputDeviceType::INPUT_DEVICE_USB,
      std::string("test external touch device"), gfx::Size(1000, 1000), 1);
}

}  // namespace

class TouchCalibratorControllerTest : public AshTestBase {
 public:
  TouchCalibratorControllerTest() = default;

  void TearDown() override {
    // Reset all touch device and touch association.
    display::test::TouchDeviceManagerTestApi(touch_device_manager())
        .ResetTouchDeviceManager();
    ui::DeviceDataManagerTestApi().SetTouchscreenDevices({});
    display::test::TouchTransformControllerTestApi(
        Shell::Get()->touch_transformer_controller())
        .touch_transform_setter()
        ->ConfigureTouchDevices(std::vector<ui::TouchDeviceTransform>());
    AshTestBase::TearDown();
  }

  display::TouchDeviceManager* touch_device_manager() {
    return display_manager()->touch_device_manager();
  }

  int GetTouchDeviceId(const TouchCalibratorController& ctrl) {
    return ctrl.touch_device_id_;
  }

  const TouchCalibratorController::CalibrationPointPairQuad& GetTouchPointQuad(
      const TouchCalibratorController& ctrl) {
    return ctrl.touch_point_quad_;
  }

  std::map<int64_t, std::unique_ptr<TouchCalibratorView>>& GetCalibratorViews(
      TouchCalibratorController* ctrl) {
    return ctrl->touch_calibrator_views_;
  }

  const display::Display& InitDisplays() {
    // Initialize 2 displays each with resolution 500x500.
    UpdateDisplay("500x500,500x500");
    // Assuming index 0 points to the native display, we will calibrate the
    // touch display at index 1.
    const int kTargetDisplayIndex = 1;
    display::DisplayIdList display_id_list =
        display_manager()->GetCurrentDisplayIdList();
    int64_t target_display_id = display_id_list[kTargetDisplayIndex];
    const display::Display& touch_display =
        display_manager()->GetDisplayForId(target_display_id);
    return touch_display;
  }

  void StartCalibrationChecks(TouchCalibratorController* ctrl,
                              const display::Display& target_display) {
    EXPECT_FALSE(ctrl->IsCalibrating());
    EXPECT_FALSE(!!ctrl->touch_calibrator_views_.size());

    TouchCalibratorController::TouchCalibrationCallback empty_callback;

    ctrl->StartCalibration(target_display, false /* is_custom_calibration */,
                           std::move(empty_callback));
    EXPECT_TRUE(ctrl->IsCalibrating());
    EXPECT_EQ(ctrl->state_,
              TouchCalibratorController::CalibrationState::kNativeCalibration);

    // There should be a touch calibrator view associated with each of the
    // active displays.
    EXPECT_EQ(ctrl->touch_calibrator_views_.size(),
              display_manager()->GetCurrentDisplayIdList().size());

    TouchCalibratorView* target_calibrator_view =
        ctrl->touch_calibrator_views_[target_display.id()].get();

    // End the background fade in animation.
    target_calibrator_view->SkipCurrentAnimation();

    // TouchCalibratorView on the display being calibrated should be at the
    // state where the first display point is visible.
    EXPECT_EQ(target_calibrator_view->state(),
              TouchCalibratorView::DISPLAY_POINT_1);
  }

  // Resets the timestamp threshold so that touch events are not discarded.
  void ResetTimestampThreshold(TouchCalibratorController* ctrl) {
    base::Time current = base::Time::Now();
    ctrl->last_touch_timestamp_ =
        current - TouchCalibratorController::kTouchIntervalThreshold;
  }

  // Generates a touch press and release event in the |display| with source
  // device id as |touch_device_id|.
  void GenerateTouchEvent(const display::Display& display,
                          int touch_device_id,
                          const gfx::Point& location = gfx::Point(20, 20)) {
    // Get the correct EventTarget for the given |display|.
    aura::Window::Windows root_windows = Shell::GetAllRootWindows();
    ui::EventTarget* event_target = nullptr;
    for (auto* window : root_windows) {
      if (display::Screen::GetScreen()->GetDisplayNearestWindow(window).id() ==
          display.id()) {
        event_target = window;
        break;
      }
    }

    ui::test::EventGenerator* eg = GetEventGenerator();
    eg->set_current_target(event_target);

    ui::TouchEvent press_touch_event(
        ui::ET_TOUCH_PRESSED, location, ui::EventTimeForNow(),
        ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 12, 1.0f,
                           1.0f, 0.0f),
        0);
    ui::TouchEvent release_touch_event(
        ui::ET_TOUCH_RELEASED, location, ui::EventTimeForNow(),
        ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 12, 1.0f,
                           1.0f, 0.0f),
        0);

    press_touch_event.set_source_device_id(touch_device_id);
    release_touch_event.set_source_device_id(touch_device_id);

    eg->Dispatch(&press_touch_event);
    eg->Dispatch(&release_touch_event);
  }

  ui::TouchscreenDevice InitTouchDevice(
      int64_t display_id,
      const ui::TouchscreenDevice& touchdevice) {
    ui::DeviceDataManagerTestApi().SetTouchscreenDevices({touchdevice});

    std::vector<ui::TouchDeviceTransform> transforms;
    ui::TouchDeviceTransform touch_device_transform;
    touch_device_transform.display_id = display_id;
    touch_device_transform.device_id = touchdevice.id;
    transforms.push_back(touch_device_transform);

    // This makes touchscreen target displays valid for ui::DeviceDataManager.
    display::test::TouchTransformControllerTestApi(
        Shell::Get()->touch_transformer_controller())
        .touch_transform_setter()
        ->ConfigureTouchDevices(transforms);
    return touchdevice;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TouchCalibratorControllerTest);
};

TEST_F(TouchCalibratorControllerTest, StartCalibration) {
  const display::Display& touch_display = InitDisplays();
  TouchCalibratorController touch_calibrator_controller;
  StartCalibrationChecks(&touch_calibrator_controller, touch_display);

  ui::EventTargetTestApi test_api(Shell::Get());
  ui::EventHandlerList handlers = test_api.GetPreTargetHandlers();
  EXPECT_TRUE(base::Contains(handlers, &touch_calibrator_controller));
}

TEST_F(TouchCalibratorControllerTest, KeyEventIntercept) {
  const display::Display& touch_display = InitDisplays();
  TouchCalibratorController touch_calibrator_controller;
  StartCalibrationChecks(&touch_calibrator_controller, touch_display);

  ui::test::EventGenerator* eg = GetEventGenerator();
  EXPECT_TRUE(touch_calibrator_controller.IsCalibrating());
  eg->PressKey(ui::VKEY_ESCAPE, ui::EF_NONE);
  EXPECT_FALSE(touch_calibrator_controller.IsCalibrating());
}

TEST_F(TouchCalibratorControllerTest, TouchThreshold) {
  const display::Display& touch_display = InitDisplays();
  TouchCalibratorController touch_calibrator_controller;
  StartCalibrationChecks(&touch_calibrator_controller, touch_display);

  // Initialize touch device so that |event_transnformer_| can be computed.
  ui::TouchscreenDevice touchdevice =
      InitTouchDevice(touch_display.id(), GetExternalTouchDevice(12));

  base::Time current_timestamp = base::Time::Now();
  touch_calibrator_controller.last_touch_timestamp_ = current_timestamp;

  // This touch event should be rejected as the time threshold has not been
  // crossed.
  GenerateTouchEvent(touch_display, touchdevice.id);

  EXPECT_EQ(touch_calibrator_controller.last_touch_timestamp_,
            current_timestamp);

  ResetTimestampThreshold(&touch_calibrator_controller);

  // This time the events should be registered as the threshold is crossed.
  GenerateTouchEvent(touch_display, touchdevice.id);

  EXPECT_LT(current_timestamp,
            touch_calibrator_controller.last_touch_timestamp_);
}

TEST_F(TouchCalibratorControllerTest, TouchDeviceIdIsSet) {
  const display::Display& touch_display = InitDisplays();

  TouchCalibratorController touch_calibrator_controller;
  StartCalibrationChecks(&touch_calibrator_controller, touch_display);

  ui::TouchscreenDevice touchdevice =
      InitTouchDevice(touch_display.id(), GetExternalTouchDevice(12));

  ResetTimestampThreshold(&touch_calibrator_controller);

  EXPECT_EQ(GetTouchDeviceId(touch_calibrator_controller),
            ui::InputDevice::kInvalidId);
  GenerateTouchEvent(touch_display, touchdevice.id);
  EXPECT_EQ(GetTouchDeviceId(touch_calibrator_controller), touchdevice.id);
}

TEST_F(TouchCalibratorControllerTest, CustomCalibration) {
  const display::Display& touch_display = InitDisplays();

  TouchCalibratorController touch_calibrator_controller;
  EXPECT_FALSE(touch_calibrator_controller.IsCalibrating());
  EXPECT_FALSE(!!GetCalibratorViews(&touch_calibrator_controller).size());

  touch_calibrator_controller.StartCalibration(
      touch_display, true /* is_custom_calibbration */,
      TouchCalibratorController::TouchCalibrationCallback());

  ui::TouchscreenDevice touchdevice =
      InitTouchDevice(touch_display.id(), GetExternalTouchDevice(12));

  EXPECT_TRUE(touch_calibrator_controller.IsCalibrating());
  EXPECT_EQ(touch_calibrator_controller.state_,
            TouchCalibratorController::CalibrationState::kCustomCalibration);

  // Native touch calibration UX should not initialize during custom calibration
  EXPECT_EQ(GetCalibratorViews(&touch_calibrator_controller).size(), 0UL);
  EXPECT_EQ(GetTouchDeviceId(touch_calibrator_controller),
            ui::InputDevice::kInvalidId);

  ResetTimestampThreshold(&touch_calibrator_controller);

  GenerateTouchEvent(touch_display, touchdevice.id);
  EXPECT_EQ(GetTouchDeviceId(touch_calibrator_controller), touchdevice.id);

  display::TouchCalibrationData::CalibrationPointPairQuad points = {
      {std::make_pair(gfx::Point(10, 10), gfx::Point(11, 12)),
       std::make_pair(gfx::Point(190, 10), gfx::Point(195, 8)),
       std::make_pair(gfx::Point(10, 90), gfx::Point(12, 94)),
       std::make_pair(gfx::Point(190, 90), gfx::Point(189, 88))}};
  gfx::Size size(200, 100);
  display::TouchCalibrationData calibration_data(points, size);

  touch_calibrator_controller.CompleteCalibration(points, size);

  const display::ManagedDisplayInfo& info =
      display_manager()->GetDisplayInfo(touch_display.id());

  display::test::TouchDeviceManagerTestApi tdm_test_api(touch_device_manager());
  EXPECT_TRUE(tdm_test_api.AreAssociated(info, touchdevice));
  EXPECT_EQ(calibration_data,
            touch_device_manager()->GetCalibrationData(touchdevice, info.id()));
}

TEST_F(TouchCalibratorControllerTest, CustomCalibrationInvalidTouchId) {
  const display::Display& touch_display = InitDisplays();

  TouchCalibratorController touch_calibrator_controller;
  EXPECT_FALSE(touch_calibrator_controller.IsCalibrating());
  EXPECT_FALSE(!!GetCalibratorViews(&touch_calibrator_controller).size());

  touch_calibrator_controller.StartCalibration(
      touch_display, true /* is_custom_calibbration */,
      TouchCalibratorController::TouchCalibrationCallback());

  EXPECT_TRUE(touch_calibrator_controller.IsCalibrating());
  EXPECT_EQ(touch_calibrator_controller.state_,
            TouchCalibratorController::CalibrationState::kCustomCalibration);

  // Native touch calibration UX should not initialize during custom calibration
  EXPECT_EQ(GetCalibratorViews(&touch_calibrator_controller).size(), 0UL);
  EXPECT_EQ(GetTouchDeviceId(touch_calibrator_controller),
            ui::InputDevice::kInvalidId);

  ResetTimestampThreshold(&touch_calibrator_controller);

  display::TouchCalibrationData::CalibrationPointPairQuad points = {
      {std::make_pair(gfx::Point(10, 10), gfx::Point(11, 12)),
       std::make_pair(gfx::Point(190, 10), gfx::Point(195, 8)),
       std::make_pair(gfx::Point(10, 90), gfx::Point(12, 94)),
       std::make_pair(gfx::Point(190, 90), gfx::Point(189, 88))}};
  gfx::Size size(200, 100);
  display::TouchCalibrationData calibration_data(points, size);

  touch_calibrator_controller.CompleteCalibration(points, size);

  const display::ManagedDisplayInfo& info =
      display_manager()->GetDisplayInfo(touch_display.id());

  ui::TouchscreenDevice random_touchdevice(
      15, ui::InputDeviceType::INPUT_DEVICE_USB,
      std::string("random touch device"), gfx::Size(123, 456), 1);
  EXPECT_EQ(calibration_data, touch_device_manager()->GetCalibrationData(
                                  random_touchdevice, info.id()));
}

TEST_F(TouchCalibratorControllerTest, IgnoreInternalTouchDevices) {
  const display::Display& touch_display = InitDisplays();

  // We need to initialize a touch device before starting calibration so that
  // the set |internal_touch_device_ids_| can be initialized.
  ui::TouchscreenDevice internal_touchdevice =
      InitTouchDevice(touch_display.id(), GetInternalTouchDevice(12));

  TouchCalibratorController touch_calibrator_controller;
  StartCalibrationChecks(&touch_calibrator_controller, touch_display);

  // We need to reinitialize the touch device as Starting calibration resets
  // everything.
  internal_touchdevice =
      InitTouchDevice(touch_display.id(), GetInternalTouchDevice(12));

  ResetTimestampThreshold(&touch_calibrator_controller);
  EXPECT_EQ(GetTouchDeviceId(touch_calibrator_controller),
            ui::InputDevice::kInvalidId);
  GenerateTouchEvent(touch_display, internal_touchdevice.id);
  EXPECT_EQ(GetTouchDeviceId(touch_calibrator_controller),
            ui::InputDevice::kInvalidId);

  ui::TouchscreenDevice external_touchdevice =
      InitTouchDevice(touch_display.id(), GetExternalTouchDevice(13));
  ResetTimestampThreshold(&touch_calibrator_controller);
  GenerateTouchEvent(touch_display, external_touchdevice.id);
  EXPECT_EQ(GetTouchDeviceId(touch_calibrator_controller),
            external_touchdevice.id);
}

TEST_F(TouchCalibratorControllerTest, HighDPIMonitorsCalibration) {
  // Initialize 3 displays each with different device scale factors.
  UpdateDisplay("500x500*2,300x300*3,500x500*1.5");

  // Index 0 points to the native internal display, we will calibrate the touch
  // display at index 2.
  const int kTargetDisplayIndex = 2;
  display::DisplayIdList display_id_list =
      display_manager()->GetCurrentDisplayIdList();

  int64_t internal_display_id = display_id_list[1];
  display::test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                         internal_display_id);

  int64_t target_display_id = display_id_list[kTargetDisplayIndex];
  const display::Display& touch_display =
      display_manager()->GetDisplayForId(target_display_id);

  // We create 2 touch devices.
  const int kInternalTouchId = 10;
  const int kExternalTouchId = 11;

  ui::TouchscreenDevice internal_touchdevice(
      kInternalTouchId, ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
      std::string("internal touch device"), gfx::Size(1000, 1000), 1);
  ui::TouchscreenDevice external_touchdevice(
      kExternalTouchId, ui::InputDeviceType::INPUT_DEVICE_USB,
      std::string("external touch device"), gfx::Size(1000, 1000), 1);

  ui::DeviceDataManagerTestApi().SetTouchscreenDevices(
      {internal_touchdevice, external_touchdevice});

  // Associate both touch devices to the internal display.
  std::vector<ui::TouchDeviceTransform> transforms;
  ui::TouchDeviceTransform touch_device_transform;
  touch_device_transform.display_id = internal_display_id;
  touch_device_transform.device_id = internal_touchdevice.id;
  transforms.push_back(touch_device_transform);

  touch_device_transform.device_id = external_touchdevice.id;
  transforms.push_back(touch_device_transform);

  display::test::TouchTransformControllerTestApi(
      Shell::Get()->touch_transformer_controller())
      .touch_transform_setter()
      ->ConfigureTouchDevices(transforms);

  // Update touch device manager with the associations.
  display::test::TouchDeviceManagerTestApi(touch_device_manager())
      .Associate(internal_display_id, internal_touchdevice);
  display::test::TouchDeviceManagerTestApi(touch_device_manager())
      .Associate(internal_display_id, external_touchdevice);

  TouchCalibratorController touch_calibrator_controller;
  touch_calibrator_controller.StartCalibration(
      touch_display, false /* is_custom_calibbration */,
      TouchCalibratorController::TouchCalibrationCallback());

  // Skip any UI animations associated with the start of calibration.
  GetCalibratorViews(&touch_calibrator_controller)[touch_display.id()]
      .get()
      ->SkipCurrentAnimation();

  // Reinitialize the transforms, as starting calibration resets them.
  display::test::TouchTransformControllerTestApi(
      Shell::Get()->touch_transformer_controller())
      .touch_transform_setter()
      ->ConfigureTouchDevices(transforms);

  // The touch device id has not been set yet, as the first touch event has not
  // been generated.
  EXPECT_EQ(GetTouchDeviceId(touch_calibrator_controller),
            ui::InputDevice::kInvalidId);

  ResetTimestampThreshold(&touch_calibrator_controller);

  // Generate a touch event at point (100, 100) on the external touch device.
  gfx::Point touch_point(100, 100);
  GenerateTouchEvent(display_manager()->GetDisplayForId(internal_display_id),
                     external_touchdevice.id, touch_point);
  EXPECT_EQ(GetTouchDeviceId(touch_calibrator_controller),
            external_touchdevice.id);

  // The touch event should be received as is.
  EXPECT_EQ(GetTouchPointQuad(touch_calibrator_controller).at(0).second,
            gfx::Point(100, 100));

  // The display point should have the root transform applied.
  EXPECT_EQ(GetTouchPointQuad(touch_calibrator_controller).at(0).first,
            gfx::Point(210, 210));
}

TEST_F(TouchCalibratorControllerTest, RotatedHighDPIMonitorsCalibration) {
  // Initialize 2 displays each with resolution 500x500. One of them at 2x
  // device scale factor.
  UpdateDisplay("500x500*2,500x500*1.5/r");

  // Index 0 points to the native internal display, we will calibrate the touch
  // display at index 1.
  const int kTargetDisplayIndex = 1;
  display::DisplayIdList display_id_list =
      display_manager()->GetCurrentDisplayIdList();

  int64_t internal_display_id = display_id_list[0];
  display::test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                         internal_display_id);

  int64_t target_display_id = display_id_list[kTargetDisplayIndex];
  const display::Display& touch_display =
      display_manager()->GetDisplayForId(target_display_id);

  // We create 2 touch devices.
  const int kInternalTouchId = 10;
  const int kExternalTouchId = 11;

  ui::TouchscreenDevice internal_touchdevice(
      kInternalTouchId, ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
      std::string("internal touch device"), gfx::Size(1000, 1000), 1);
  ui::TouchscreenDevice external_touchdevice(
      kExternalTouchId, ui::InputDeviceType::INPUT_DEVICE_USB,
      std::string("external touch device"), gfx::Size(1000, 1000), 1);

  ui::DeviceDataManagerTestApi().SetTouchscreenDevices(
      {internal_touchdevice, external_touchdevice});

  // Associate both touch devices to the internal display.
  std::vector<ui::TouchDeviceTransform> transforms;
  ui::TouchDeviceTransform touch_device_transform;
  touch_device_transform.display_id = internal_display_id;
  touch_device_transform.device_id = internal_touchdevice.id;
  transforms.push_back(touch_device_transform);

  touch_device_transform.device_id = external_touchdevice.id;
  transforms.push_back(touch_device_transform);

  display::test::TouchTransformControllerTestApi(
      Shell::Get()->touch_transformer_controller())
      .touch_transform_setter()
      ->ConfigureTouchDevices(transforms);

  // Update touch device manager with the associations.
  display::test::TouchDeviceManagerTestApi(touch_device_manager())
      .Associate(internal_display_id, internal_touchdevice);
  display::test::TouchDeviceManagerTestApi(touch_device_manager())
      .Associate(internal_display_id, external_touchdevice);

  TouchCalibratorController touch_calibrator_controller;
  touch_calibrator_controller.StartCalibration(
      touch_display, false /* is_custom_calibbration */,
      TouchCalibratorController::TouchCalibrationCallback());

  // Skip any UI animations associated with the start of calibration.
  GetCalibratorViews(&touch_calibrator_controller)[touch_display.id()]
      .get()
      ->SkipCurrentAnimation();

  // Reinitialize the transforms, as starting calibration resets them.
  display::test::TouchTransformControllerTestApi(
      Shell::Get()->touch_transformer_controller())
      .touch_transform_setter()
      ->ConfigureTouchDevices(transforms);

  // The touch device id has not been set yet, as the first touch event has not
  // been generated.
  EXPECT_EQ(GetTouchDeviceId(touch_calibrator_controller),
            ui::InputDevice::kInvalidId);

  ResetTimestampThreshold(&touch_calibrator_controller);

  // Generate a touch event at point (100, 100) on the external touch device.
  gfx::Point touch_point(100, 100);

  // Simulate the touch event for external touch device. Since the device is
  // currently associated with the internal display, we generate the touch event
  // for the internal display.
  GenerateTouchEvent(display_manager()->GetDisplayForId(internal_display_id),
                     external_touchdevice.id, touch_point);
  EXPECT_EQ(GetTouchDeviceId(touch_calibrator_controller),
            external_touchdevice.id);

  // The touch event should now be within the bounds the the target display.
  EXPECT_EQ(GetTouchPointQuad(touch_calibrator_controller).at(0).second,
            gfx::Point(100, 100));

  // The display point should have the root transform applied.
  EXPECT_EQ(GetTouchPointQuad(touch_calibrator_controller).at(0).first,
            gfx::Point(290, 210));
}

TEST_F(TouchCalibratorControllerTest, InternalTouchDeviceIsRejected) {
  const display::Display& touch_display = InitDisplays();

  // We need to initialize a touch device before starting calibration so that
  // the set |internal_touch_device_ids_| can be initialized.
  ui::TouchscreenDevice internal_touchdevice =
      InitTouchDevice(touch_display.id(), GetInternalTouchDevice(12));

  TouchCalibratorController touch_calibrator_controller;
  touch_calibrator_controller.touch_device_id_ = internal_touchdevice.id;
  touch_calibrator_controller.target_display_ = touch_display;

  display::TouchCalibrationData::CalibrationPointPairQuad points = {
      {std::make_pair(gfx::Point(10, 10), gfx::Point(11, 12)),
       std::make_pair(gfx::Point(190, 10), gfx::Point(195, 8)),
       std::make_pair(gfx::Point(10, 90), gfx::Point(12, 94)),
       std::make_pair(gfx::Point(190, 90), gfx::Point(189, 88))}};
  gfx::Size size(200, 100);
  display::TouchCalibrationData calibration_data(points, size);

  touch_calibrator_controller.CompleteCalibration(points, size);

  const display::ManagedDisplayInfo& info =
      display_manager()->GetDisplayInfo(touch_display.id());

  display::test::TouchDeviceManagerTestApi tdm_test_api(touch_device_manager());
  EXPECT_FALSE(tdm_test_api.AreAssociated(info, internal_touchdevice));
  EXPECT_TRUE(touch_device_manager()
                  ->GetCalibrationData(internal_touchdevice, info.id())
                  .IsEmpty());
}

}  // namespace ash
