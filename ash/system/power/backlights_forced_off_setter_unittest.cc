// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/backlights_forced_off_setter.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/shell.h"
#include "ash/system/power/scoped_backlights_forced_off.h"
#include "ash/test/ash_test_base.h"
#include "ash/touch/ash_touch_transform_controller.h"
#include "ash/touch/touch_devices_controller.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/test/touch_transform_controller_test_api.h"
#include "ui/display/manager/touch_transform_setter.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/devices/touch_device_transform.h"
#include "ui/events/devices/touchscreen_device.h"

namespace ash {

namespace {

class TestObserver : public BacklightsForcedOffSetter::Observer {
 public:
  explicit TestObserver(BacklightsForcedOffSetter* backlights_forced_off_setter)
      : backlights_forced_off_setter_(backlights_forced_off_setter),
        scoped_observer_(this) {
    scoped_observer_.Add(backlights_forced_off_setter);
  }

  ~TestObserver() override = default;

  const std::vector<bool>& forced_off_states() const {
    return forced_off_states_;
  }

  void ClearForcedOffStates() { forced_off_states_.clear(); }

  // BacklightsForcedOffSetter::Observer:
  void OnBacklightsForcedOffChanged(bool backlights_forced_off) override {
    ASSERT_EQ(backlights_forced_off,
              backlights_forced_off_setter_->backlights_forced_off());
    forced_off_states_.push_back(backlights_forced_off);
  }

 private:
  BacklightsForcedOffSetter* const backlights_forced_off_setter_;

  std::vector<bool> forced_off_states_;

  ScopedObserver<BacklightsForcedOffSetter, BacklightsForcedOffSetter::Observer>
      scoped_observer_;

  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

}  // namespace

class BacklightsForcedOffSetterTest : public AshTestBase {
 public:
  BacklightsForcedOffSetterTest() = default;
  ~BacklightsForcedOffSetterTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    backlights_forced_off_setter_ =
        std::make_unique<BacklightsForcedOffSetter>();
    backlights_forced_off_observer_ =
        std::make_unique<TestObserver>(backlights_forced_off_setter_.get());
  }

  void TearDown() override {
    backlights_forced_off_observer_.reset();
    backlights_forced_off_setter_.reset();
    AshTestBase::TearDown();
  }

  void ResetBacklightsForcedOffSetter() {
    backlights_forced_off_observer_.reset();
    backlights_forced_off_setter_.reset();
  }

 protected:
  std::unique_ptr<BacklightsForcedOffSetter> backlights_forced_off_setter_;
  std::unique_ptr<TestObserver> backlights_forced_off_observer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BacklightsForcedOffSetterTest);
};

TEST_F(BacklightsForcedOffSetterTest, SingleForcedOffRequest) {
  ASSERT_FALSE(power_manager_client()->backlights_forced_off());

  std::unique_ptr<ScopedBacklightsForcedOff> scoped_forced_off =
      backlights_forced_off_setter_->ForceBacklightsOff();

  EXPECT_TRUE(power_manager_client()->backlights_forced_off());
  EXPECT_EQ(std::vector<bool>({true}),
            backlights_forced_off_observer_->forced_off_states());
  backlights_forced_off_observer_->ClearForcedOffStates();

  scoped_forced_off.reset();

  EXPECT_FALSE(power_manager_client()->backlights_forced_off());
  EXPECT_EQ(std::vector<bool>({false}),
            backlights_forced_off_observer_->forced_off_states());
}

TEST_F(BacklightsForcedOffSetterTest, BacklightsForcedOffSetterDeleted) {
  ASSERT_FALSE(power_manager_client()->backlights_forced_off());

  std::unique_ptr<ScopedBacklightsForcedOff> scoped_forced_off =
      backlights_forced_off_setter_->ForceBacklightsOff();

  EXPECT_TRUE(power_manager_client()->backlights_forced_off());
  EXPECT_EQ(std::vector<bool>({true}),
            backlights_forced_off_observer_->forced_off_states());
  backlights_forced_off_observer_->ClearForcedOffStates();

  ResetBacklightsForcedOffSetter();

  EXPECT_FALSE(power_manager_client()->backlights_forced_off());

  // Verify that deleting scoped forced off request does not affect
  // power manager state (nor cause a crash).
  scoped_forced_off.reset();
  EXPECT_FALSE(power_manager_client()->backlights_forced_off());
}

TEST_F(BacklightsForcedOffSetterTest,
       OverlappingRequests_SecondRequestResetFirst) {
  ASSERT_FALSE(power_manager_client()->backlights_forced_off());

  std::unique_ptr<ScopedBacklightsForcedOff> scoped_forced_off_1 =
      backlights_forced_off_setter_->ForceBacklightsOff();

  EXPECT_TRUE(power_manager_client()->backlights_forced_off());
  EXPECT_EQ(std::vector<bool>({true}),
            backlights_forced_off_observer_->forced_off_states());
  backlights_forced_off_observer_->ClearForcedOffStates();

  std::unique_ptr<ScopedBacklightsForcedOff> scoped_forced_off_2 =
      backlights_forced_off_setter_->ForceBacklightsOff();

  EXPECT_TRUE(power_manager_client()->backlights_forced_off());
  EXPECT_TRUE(backlights_forced_off_observer_->forced_off_states().empty());

  scoped_forced_off_2.reset();

  EXPECT_TRUE(power_manager_client()->backlights_forced_off());
  EXPECT_TRUE(backlights_forced_off_observer_->forced_off_states().empty());

  scoped_forced_off_1.reset();

  EXPECT_FALSE(power_manager_client()->backlights_forced_off());
  EXPECT_EQ(std::vector<bool>({false}),
            backlights_forced_off_observer_->forced_off_states());
}

TEST_F(BacklightsForcedOffSetterTest,
       OverlappingRequests_FirstRequestResetFirst) {
  ASSERT_FALSE(power_manager_client()->backlights_forced_off());

  std::unique_ptr<ScopedBacklightsForcedOff> scoped_forced_off_1 =
      backlights_forced_off_setter_->ForceBacklightsOff();

  EXPECT_TRUE(power_manager_client()->backlights_forced_off());
  EXPECT_EQ(std::vector<bool>({true}),
            backlights_forced_off_observer_->forced_off_states());
  backlights_forced_off_observer_->ClearForcedOffStates();

  std::unique_ptr<ScopedBacklightsForcedOff> scoped_forced_off_2 =
      backlights_forced_off_setter_->ForceBacklightsOff();

  EXPECT_TRUE(power_manager_client()->backlights_forced_off());
  EXPECT_TRUE(backlights_forced_off_observer_->forced_off_states().empty());

  scoped_forced_off_1.reset();

  EXPECT_TRUE(backlights_forced_off_observer_->forced_off_states().empty());

  scoped_forced_off_2.reset();

  EXPECT_FALSE(power_manager_client()->backlights_forced_off());
  EXPECT_EQ(std::vector<bool>({false}),
            backlights_forced_off_observer_->forced_off_states());
}

TEST_F(BacklightsForcedOffSetterTest,
       ExternalTouchDevicePreventsTouchscreenDisable) {
  UpdateDisplay("1200x600,600x1000*2");
  const int64_t kInternalDisplayId =
      display::test::DisplayManagerTestApi(display_manager())
          .SetFirstDisplayAsInternalDisplay();

  display::DisplayIdList display_id_list =
      display_manager()->GetCurrentDisplayIdList();

  // Pick the non internal display Id.
  const int64_t kExternalDisplayId = display_id_list[0] == kInternalDisplayId
                                         ? display_id_list[1]
                                         : display_id_list[0];

  // Initialize an external touch device.
  ui::TouchscreenDevice external_touchdevice(
      123, ui::InputDeviceType::INPUT_DEVICE_USB,
      std::string("test external touch device"), gfx::Size(1000, 1000), 1);

  ui::TouchscreenDevice internal_touchdevice(
      234, ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
      std::string("test internal touch device"), gfx::Size(1000, 1000), 1);
  ui::DeviceDataManagerTestApi().SetTouchscreenDevices(
      {external_touchdevice, internal_touchdevice});

  std::vector<ui::TouchDeviceTransform> transforms;
  ui::TouchDeviceTransform touch_device_transform;

  // Add external touch device to the list.
  touch_device_transform.display_id = kExternalDisplayId;
  touch_device_transform.device_id = external_touchdevice.id;
  transforms.push_back(touch_device_transform);

  // Add internal touch device to the list.
  touch_device_transform.display_id = kInternalDisplayId;
  touch_device_transform.device_id = internal_touchdevice.id;
  transforms.push_back(touch_device_transform);

  // Initialize the transforms and the DeviceDataManager.
  display::test::TouchTransformControllerTestApi(
      ash::Shell::Get()->touch_transformer_controller())
      .touch_transform_setter()
      ->ConfigureTouchDevices(transforms);

  // The brightness change is due to user inactivity or lid close.
  power_manager::BacklightBrightnessChange change;
  change.set_cause(power_manager::BacklightBrightnessChange_Cause_OTHER);
  change.set_percent(0.0);

  power_manager_client()->SendScreenBrightnessChanged(change);

  // The touchscreens should be enabled.
  EXPECT_TRUE(Shell::Get()->touch_devices_controller()->GetTouchscreenEnabled(
      TouchDeviceEnabledSource::GLOBAL));
}

TEST_F(BacklightsForcedOffSetterTest, TouchscreensDisableOnBrightnessChange) {
  UpdateDisplay("1200x600,600x1000*2");
  const int64_t kInternalDisplayId =
      display::test::DisplayManagerTestApi(display_manager())
          .SetFirstDisplayAsInternalDisplay();

  display::DisplayIdList display_id_list =
      display_manager()->GetCurrentDisplayIdList();

  ui::TouchscreenDevice internal_touchdevice(
      234, ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
      std::string("test internal touch device"), gfx::Size(1000, 1000), 1);
  ui::DeviceDataManagerTestApi().SetTouchscreenDevices({internal_touchdevice});

  // Add internal touch device to the list.
  std::vector<ui::TouchDeviceTransform> transforms;
  ui::TouchDeviceTransform touch_device_transform;
  touch_device_transform.display_id = kInternalDisplayId;
  touch_device_transform.device_id = internal_touchdevice.id;
  transforms.push_back(touch_device_transform);

  // Initialize the transforms and the DeviceDataManager.
  display::test::TouchTransformControllerTestApi(
      ash::Shell::Get()->touch_transformer_controller())
      .touch_transform_setter()
      ->ConfigureTouchDevices(transforms);

  // The brightness change is due to user inactivity or lid close.
  power_manager::BacklightBrightnessChange change;
  change.set_cause(power_manager::BacklightBrightnessChange_Cause_OTHER);
  change.set_percent(0.0);

  power_manager_client()->SendScreenBrightnessChanged(change);

  // The touchscreens should be disabled.
  EXPECT_FALSE(Shell::Get()->touch_devices_controller()->GetTouchscreenEnabled(
      TouchDeviceEnabledSource::GLOBAL));
}

}  // namespace ash
