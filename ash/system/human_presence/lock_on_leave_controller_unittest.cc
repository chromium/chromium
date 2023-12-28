// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/human_presence/lock_on_leave_controller.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/test/ash_test_base.h"
#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/dbus/human_presence/fake_human_presence_dbus_client.h"
#include "chromeos/ash/components/dbus/human_presence/human_presence_dbus_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class LockOnLeaveControllerTest : public AshTestBase {
 public:
  void SetUp() override {
    // We need to enable kQuickDim to construct
    // HumanPresenceOrientationController.
    scoped_feature_list_.InitAndEnableFeature(features::kQuickDim);
    base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kHasHps);

    // Initialize FakeHumanPresenceDBusClient.
    HumanPresenceDBusClient::InitializeFake();
    human_presence_client_ = FakeHumanPresenceDBusClient::Get();
    human_presence_client_->Reset();

    AshTestBase::SetUp();
  }

 protected:
  raw_ptr<FakeHumanPresenceDBusClient> human_presence_client_ = nullptr;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::ScopedCommandLine scoped_command_line_;
};

// EnableLockOnLeave should be skipped if the service is not available.
TEST_F(LockOnLeaveControllerTest,
       EnableLockOnLeaveDoesNothingIfServiceUnavailable) {
  auto lock_on_leave_controller = std::make_unique<LockOnLeaveController>();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(human_presence_client_->disable_hps_sense_count(), 0);
  EXPECT_EQ(human_presence_client_->enable_hps_sense_count(), 0);

  lock_on_leave_controller->EnableLockOnLeave();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(human_presence_client_->disable_hps_sense_count(), 0);
  EXPECT_EQ(human_presence_client_->enable_hps_sense_count(), 0);
}

// DisableLockOnLeave should be called when the human presence service becomes
// available.
TEST_F(LockOnLeaveControllerTest, CallDisableLockOnLeaveOnServiceAvailable) {
  human_presence_client_->set_hps_service_is_available(true);
  auto lock_on_leave_controller = std::make_unique<LockOnLeaveController>();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(human_presence_client_->disable_hps_sense_count(), 1);
  EXPECT_EQ(human_presence_client_->enable_hps_sense_count(), 0);
}

// EnableLockOnLeave should succeed if the service is available.
TEST_F(LockOnLeaveControllerTest, EnableLockOnLeaveSucceedsIfServiceAvailable) {
  human_presence_client_->set_hps_service_is_available(true);
  auto lock_on_leave_controller = std::make_unique<LockOnLeaveController>();
  base::RunLoop().RunUntilIdle();
  human_presence_client_->Reset();

  lock_on_leave_controller->EnableLockOnLeave();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(human_presence_client_->disable_hps_sense_count(), 0);
  EXPECT_EQ(human_presence_client_->enable_hps_sense_count(), 1);
}

// The DBus EnableHpsSense method should only be called once for multiple calls
// of EnableLockOnLeave.
TEST_F(LockOnLeaveControllerTest, EnableHpsSenseOnlyCalledOnceOnTwoCalls) {
  human_presence_client_->set_hps_service_is_available(true);
  auto lock_on_leave_controller = std::make_unique<LockOnLeaveController>();
  base::RunLoop().RunUntilIdle();
  human_presence_client_->Reset();

  // Only 1 dbus calls should be sent.
  lock_on_leave_controller->EnableLockOnLeave();
  lock_on_leave_controller->EnableLockOnLeave();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(human_presence_client_->disable_hps_sense_count(), 0);
  EXPECT_EQ(human_presence_client_->enable_hps_sense_count(), 1);
}

TEST_F(LockOnLeaveControllerTest, DisableLockOnLeaveDoesNothingIfNotEnabled) {
  human_presence_client_->set_hps_service_is_available(true);
  auto lock_on_leave_controller = std::make_unique<LockOnLeaveController>();
  base::RunLoop().RunUntilIdle();
  human_presence_client_->Reset();

  // Calls DisableLockOnLeave does nothing if LockOnLeave is not enabled.
  lock_on_leave_controller->DisableLockOnLeave();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(human_presence_client_->disable_hps_sense_count(), 0);
  EXPECT_EQ(human_presence_client_->enable_hps_sense_count(), 0);
}

// DisableLockOnLeave should succeed if LockOnLeave is enabled.
TEST_F(LockOnLeaveControllerTest, DisableLockOnLeaveSuceedsIfEnabled) {
  human_presence_client_->set_hps_service_is_available(true);
  auto lock_on_leave_controller = std::make_unique<LockOnLeaveController>();
  lock_on_leave_controller->EnableLockOnLeave();
  human_presence_client_->Reset();

  // DisableLockOnLeave succeeds since LockOnLeave is enabled.
  lock_on_leave_controller->DisableLockOnLeave();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(human_presence_client_->disable_hps_sense_count(), 1);
  EXPECT_EQ(human_presence_client_->enable_hps_sense_count(), 0);
}

// The DBus method DisableHpsSense should only be called once with two
// consecutive calls to DisableLockOnLeave.
TEST_F(LockOnLeaveControllerTest, DisableHpsSenseOnlyCalledOnceOnTwoCalls) {
  human_presence_client_->set_hps_service_is_available(true);
  auto lock_on_leave_controller = std::make_unique<LockOnLeaveController>();
  lock_on_leave_controller->EnableLockOnLeave();
  base::RunLoop().RunUntilIdle();
  human_presence_client_->Reset();

  // Only 1 dbus calls should be sent.
  lock_on_leave_controller->DisableLockOnLeave();
  lock_on_leave_controller->DisableLockOnLeave();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(human_presence_client_->disable_hps_sense_count(), 1);
  EXPECT_EQ(human_presence_client_->enable_hps_sense_count(), 0);
}

// No dbus call should be sent on restart if LockOnLeave is currently disabled.
TEST_F(LockOnLeaveControllerTest, NoDbusCallsOnRestartIfLockOnLeaveDisabled) {
  human_presence_client_->set_hps_service_is_available(true);
  auto lock_on_leave_controller = std::make_unique<LockOnLeaveController>();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(human_presence_client_->disable_hps_sense_count(), 1);
  EXPECT_EQ(human_presence_client_->enable_hps_sense_count(), 0);

  // LockOnLeave is disabled; Restart will not send dbus calls.
  human_presence_client_->Shutdown();
  human_presence_client_->Restart();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(human_presence_client_->disable_hps_sense_count(), 1);
  EXPECT_EQ(human_presence_client_->enable_hps_sense_count(), 0);
}

// The DBus method EnableHpsSense should be called on restart if LockOnLeave is
// enabled currently.
TEST_F(LockOnLeaveControllerTest,
       EnableLockOnLeaveOnRestartIfLockOnLeaveWasEnabled) {
  human_presence_client_->set_hps_service_is_available(true);
  auto lock_on_leave_controller = std::make_unique<LockOnLeaveController>();
  base::RunLoop().RunUntilIdle();
  lock_on_leave_controller->EnableLockOnLeave();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(human_presence_client_->disable_hps_sense_count(), 1);
  EXPECT_EQ(human_presence_client_->enable_hps_sense_count(), 1);

  // LockOnLeave is enabled; Restart will call EnableHpsSense.
  human_presence_client_->Shutdown();
  human_presence_client_->Restart();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(human_presence_client_->disable_hps_sense_count(), 1);
  EXPECT_EQ(human_presence_client_->enable_hps_sense_count(), 2);
}

// The DBus method DisableHpsSense should be called on service available even if
// we need to enable it immediately after that.
TEST_F(LockOnLeaveControllerTest, AlwaysCallDisableOnServiceAvailable) {
  human_presence_client_->set_hps_service_is_available(true);
  auto lock_on_leave_controller = std::make_unique<LockOnLeaveController>();
  lock_on_leave_controller->EnableLockOnLeave();
  // At this point, OnServiceAvailable is not called yet, so no disable/enable
  // functions should be called.
  EXPECT_EQ(human_presence_client_->disable_hps_sense_count(), 0);
  EXPECT_EQ(human_presence_client_->enable_hps_sense_count(), 0);
  base::RunLoop().RunUntilIdle();
  // Although we only need enabling, the disable function should also be called.
  EXPECT_EQ(human_presence_client_->disable_hps_sense_count(), 1);
  EXPECT_EQ(human_presence_client_->enable_hps_sense_count(), 1);
}

// Confirm that the DBus method EnableHpsSense is only called when both
// EnableLockOnLeave and OnOrientationChanged(true) is set.
TEST_F(LockOnLeaveControllerTest, OrientationChanging) {
  human_presence_client_->set_hps_service_is_available(true);
  auto lock_on_leave_controller = std::make_unique<LockOnLeaveController>();
  lock_on_leave_controller->EnableLockOnLeave();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(human_presence_client_->disable_hps_sense_count(), 1);
  EXPECT_EQ(human_presence_client_->enable_hps_sense_count(), 1);
  human_presence_client_->Reset();

  // Orientation changed, LockOnLeave should be disabled.
  lock_on_leave_controller->OnOrientationChanged(false);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(human_presence_client_->disable_hps_sense_count(), 1);
  EXPECT_EQ(human_presence_client_->enable_hps_sense_count(), 0);
  human_presence_client_->Reset();

  // Calling enable/disable will not sent any dbus call while OrientationChanged
  // to be false.
  lock_on_leave_controller->DisableLockOnLeave();
  base::RunLoop().RunUntilIdle();
  lock_on_leave_controller->OnOrientationChanged(false);
  base::RunLoop().RunUntilIdle();
  lock_on_leave_controller->EnableLockOnLeave();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(human_presence_client_->disable_hps_sense_count(), 0);
  EXPECT_EQ(human_presence_client_->enable_hps_sense_count(), 0);

  // Changing orientation will trigger enabling if EnableLockOnLeave was set.
  lock_on_leave_controller->OnOrientationChanged(true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(human_presence_client_->disable_hps_sense_count(), 0);
  EXPECT_EQ(human_presence_client_->enable_hps_sense_count(), 1);
}

}  // namespace ash
