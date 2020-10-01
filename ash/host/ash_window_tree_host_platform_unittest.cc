// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/host/ash_window_tree_host_platform.h"

#include "ash/test/ash_test_base.h"

#include "ui/ozone/public/input_controller.h"
#include "ui/ozone/public/ozone_platform.h"

namespace ash {

class AshWindowTreeHostPlatformTest : public AshTestBase {
 public:
  AshWindowTreeHostPlatformTest() = default;
  ~AshWindowTreeHostPlatformTest() override = default;
};

class TestInputController : public ui::InputController {
 public:
  TestInputController() = default;
  ~TestInputController() override = default;

  // InputController:
  void SuspendMouseAcceleration() override { acceleration_suspended_ = true; }
  void EndMouseAccelerationSuspension() override {
    acceleration_suspended_ = false;
  }

  // these are all stubs that are not used yet in these tests
  bool HasMouse() override { return false; }
  bool HasPointingStick() override { return false; }
  bool HasTouchpad() override { return false; }
  bool IsCapsLockEnabled() override { return false; }
  void SetCapsLockEnabled(bool enabled) override {}
  void SetNumLockEnabled(bool enabled) override {}
  bool IsAutoRepeatEnabled() override { return true; }
  void SetAutoRepeatEnabled(bool enabled) override {}
  void SetAutoRepeatRate(const base::TimeDelta& delay,
                         const base::TimeDelta& interval) override {}
  void GetAutoRepeatRate(base::TimeDelta* delay,
                         base::TimeDelta* interval) override {}
  void SetCurrentLayoutByName(const std::string& layout_name) override {}
  void SetTouchEventLoggingEnabled(bool enabled) override {
    NOTIMPLEMENTED_LOG_ONCE();
  }
  void SetTouchpadSensitivity(int value) override {}
  void SetTouchpadScrollSensitivity(int value) override {}
  void SetTapToClick(bool enabled) override {}
  void SetThreeFingerClick(bool enabled) override {}
  void SetTapDragging(bool enabled) override {}
  void SetNaturalScroll(bool enabled) override {}
  void SetMouseSensitivity(int value) override {}
  void SetMouseScrollSensitivity(int value) override {}
  void SetPrimaryButtonRight(bool right) override {}
  void SetMouseReverseScroll(bool enabled) override {}
  void SetMouseAcceleration(bool enabled) override {}
  void SetMouseScrollAcceleration(bool enabled) override {}
  void SetTouchpadAcceleration(bool enabled) override {}
  void SetTouchpadScrollAcceleration(bool enabled) override {}
  void SetTapToClickPaused(bool state) override {}
  void GetTouchDeviceStatus(GetTouchDeviceStatusReply reply) override {
    std::move(reply).Run(std::string());
  }
  void GetTouchEventLog(const base::FilePath& out_dir,
                        GetTouchEventLogReply reply) override {
    std::move(reply).Run(std::vector<base::FilePath>());
  }
  void PlayVibrationEffect(int id,
                           uint8_t amplitude,
                           uint16_t duration_millis) override {}
  void StopVibration(int id) override {}
  void SetInternalTouchpadEnabled(bool enabled) override {}
  bool IsInternalTouchpadEnabled() const override { return false; }
  void SetTouchscreensEnabled(bool enabled) override {}
  void SetInternalKeyboardFilter(
      bool enable_filter,
      std::vector<ui::DomCode> allowed_keys) override {}
  void GetGesturePropertiesService(
      mojo::PendingReceiver<ui::ozone::mojom::GesturePropertiesService>
          receiver) override {}

  bool GetAccelerationSuspended() { return acceleration_suspended_; }

 private:
  // member variable used to keep track of mouse acceleration suspension
  bool acceleration_suspended_ = false;
};

TEST_F(AshWindowTreeHostPlatformTest, UnadjustedMovement) {
  AshWindowTreeHostPlatform host;
  auto test_input_controller = std::make_unique<TestInputController>();
  host.input_controller_ = test_input_controller.get();

  std::unique_ptr<aura::ScopedEnableUnadjustedMouseEvents>
      unadjusted_movement_context = host.RequestUnadjustedMovement();
  EXPECT_TRUE(unadjusted_movement_context.get() != nullptr);
  EXPECT_TRUE(test_input_controller->GetAccelerationSuspended());
  unadjusted_movement_context.reset();
  EXPECT_FALSE(test_input_controller->GetAccelerationSuspended());
}

}  // namespace ash
