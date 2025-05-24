// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/host/ash_window_tree_host_platform.h"

#include <memory>

#include "ash/host/ash_window_tree_host_delegate.h"
#include "ash/test/ash_test_base.h"
#include "ui/events/devices/haptic_touchpad_effects.h"
#include "ui/events/devices/stylus_state.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/stub_input_controller.h"
#include "ui/platform_window/stub/stub_window.h"

namespace ash {

namespace {

class TestInputController : public ui::StubInputController {
 public:
  TestInputController() = default;
  TestInputController(const TestInputController&) = delete;
  TestInputController& operator=(const TestInputController&) = delete;
  ~TestInputController() override = default;

  // InputController:
  void SuspendMouseAcceleration() override { acceleration_suspended_ = true; }
  void EndMouseAccelerationSuspension() override {
    acceleration_suspended_ = false;
  }

  bool GetAccelerationSuspended() { return acceleration_suspended_; }

 private:
  // member variable used to keep track of mouse acceleration suspension
  bool acceleration_suspended_ = false;
};

class FakeAshWindowTreeHostDelegate : public AshWindowTreeHostDelegate {
 public:
  FakeAshWindowTreeHostDelegate() = default;
  FakeAshWindowTreeHostDelegate(const FakeAshWindowTreeHostDelegate&) = delete;
  FakeAshWindowTreeHostDelegate& operator=(
      const FakeAshWindowTreeHostDelegate&) = delete;
  ~FakeAshWindowTreeHostDelegate() override = default;

  const display::Display* GetDisplayById(int64_t display_id) const override {
    return nullptr;
  }
  void SetCurrentEventTargeterSourceHost(aura::WindowTreeHost*) override {}
};

}  // namespace

class AshWindowTreeHostPlatformTest : public AshTestBase {
 public:
  AshWindowTreeHostPlatformTest() = default;
  AshWindowTreeHostPlatformTest(const AshWindowTreeHostPlatformTest&) = delete;
  AshWindowTreeHostPlatformTest& operator=(
      const AshWindowTreeHostPlatformTest&) = delete;
  ~AshWindowTreeHostPlatformTest() override = default;
};

TEST_F(AshWindowTreeHostPlatformTest, UnadjustedMovement) {
  FakeAshWindowTreeHostDelegate fake_delegate;
  auto stub = std::make_unique<ui::StubWindow>(gfx::Rect());
  auto* stub_ptr = stub.get();
  AshWindowTreeHostPlatform host(std::move(stub), &fake_delegate);
  stub_ptr->InitDelegate(&host, false);

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
