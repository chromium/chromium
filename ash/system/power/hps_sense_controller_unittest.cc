// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/hps_sense_controller.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/dbus/hps/fake_hps_dbus_client.h"
#include "chromeos/dbus/hps/hps_dbus_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class HpsSenseControllerTest : public testing::Test {
 public:
  void SetUp() override {
    chromeos::HpsDBusClient::InitializeFake();
    hps_client_ = chromeos::FakeHpsDBusClient::Get();
    hps_client_->Reset();
  }

 protected:
  chromeos::FakeHpsDBusClient* hps_client_ = nullptr;
  base::test::SingleThreadTaskEnvironment task_environment_;
};

// EnableHpsSense should be skipped if HpsService is not available.
TEST_F(HpsSenseControllerTest,
       EnableHpsSenseDoesNothingIfHpsServiceUnavailable) {
  auto hps_sense_controller = std::make_unique<HpsSenseController>();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(hps_client_->disable_hps_sense_count(), 0);
  EXPECT_EQ(hps_client_->enable_hps_sense_count(), 0);

  hps_sense_controller->EnableHpsSense();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(hps_client_->disable_hps_sense_count(), 0);
  EXPECT_EQ(hps_client_->enable_hps_sense_count(), 0);
}

// DisableHpsSense should be called on HpsServiceAvailable.
TEST_F(HpsSenseControllerTest, CallDisableHpsSenseOnHpsServiceAvailable) {
  hps_client_->set_hps_service_is_available(true);
  auto hps_sense_controller = std::make_unique<HpsSenseController>();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(hps_client_->disable_hps_sense_count(), 1);
  EXPECT_EQ(hps_client_->enable_hps_sense_count(), 0);
}

// EnableHpsSense should succeed if HpsService is available.
TEST_F(HpsSenseControllerTest, EnableHpsSenseSucceedsIfHpsServiceAvailable) {
  hps_client_->set_hps_service_is_available(true);
  auto hps_sense_controller = std::make_unique<HpsSenseController>();
  base::RunLoop().RunUntilIdle();
  hps_client_->Reset();

  hps_sense_controller->EnableHpsSense();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(hps_client_->disable_hps_sense_count(), 0);
  EXPECT_EQ(hps_client_->enable_hps_sense_count(), 1);
}

// EnableHpsSense should only be applied once for multiple calls of
// EnableHpsSense.
TEST_F(HpsSenseControllerTest, EnableHpsSenseOnlyCalledOnceOnTwoCalls) {
  hps_client_->set_hps_service_is_available(true);
  auto hps_sense_controller = std::make_unique<HpsSenseController>();
  base::RunLoop().RunUntilIdle();
  hps_client_->Reset();

  // Only 1 dbus calls should be sent.
  hps_sense_controller->EnableHpsSense();
  hps_sense_controller->EnableHpsSense();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(hps_client_->disable_hps_sense_count(), 0);
  EXPECT_EQ(hps_client_->enable_hps_sense_count(), 1);
}

TEST_F(HpsSenseControllerTest, DisableHpsSenseDoesNothingIfNotEnabled) {
  hps_client_->set_hps_service_is_available(true);
  auto hps_sense_controller = std::make_unique<HpsSenseController>();
  base::RunLoop().RunUntilIdle();
  hps_client_->Reset();

  // Calls DisableHpsSense does nothing if HpsSense is not enabled.
  hps_sense_controller->DisableHpsSense();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(hps_client_->disable_hps_sense_count(), 0);
  EXPECT_EQ(hps_client_->enable_hps_sense_count(), 0);
}

// DisableHpsSense should succeed if HpsSense is enabled.
TEST_F(HpsSenseControllerTest, DisableHpsSenseSuceedsIfEnabled) {
  hps_client_->set_hps_service_is_available(true);
  auto hps_sense_controller = std::make_unique<HpsSenseController>();
  hps_sense_controller->EnableHpsSense();
  hps_client_->Reset();

  // DisableHpsSense succeeds since HpsSense is enabled.
  hps_sense_controller->DisableHpsSense();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(hps_client_->disable_hps_sense_count(), 1);
  EXPECT_EQ(hps_client_->enable_hps_sense_count(), 0);
}

// DisableHpsSense should only be applied once with two consecutive calls.
TEST_F(HpsSenseControllerTest, DisableHpsSenseOnlyCalledOnceOnTwoCalls) {
  hps_client_->set_hps_service_is_available(true);
  auto hps_sense_controller = std::make_unique<HpsSenseController>();
  hps_sense_controller->EnableHpsSense();
  base::RunLoop().RunUntilIdle();
  hps_client_->Reset();

  // Only 1 dbus calls should be sent.
  hps_sense_controller->DisableHpsSense();
  hps_sense_controller->DisableHpsSense();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(hps_client_->disable_hps_sense_count(), 1);
  EXPECT_EQ(hps_client_->enable_hps_sense_count(), 0);
}

// EnableHpsSense should be called on restart if HpsSense is enabled currently.
TEST_F(HpsSenseControllerTest, NoDbusCallsOnRestartIfHpsSenseDisabled) {
  hps_client_->set_hps_service_is_available(true);
  auto hps_sense_controller = std::make_unique<HpsSenseController>();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(hps_client_->disable_hps_sense_count(), 1);
  EXPECT_EQ(hps_client_->enable_hps_sense_count(), 0);

  // HpsSense is disabled; Restart will not send dbus calls.
  hps_client_->Restart();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(hps_client_->disable_hps_sense_count(), 1);
  EXPECT_EQ(hps_client_->enable_hps_sense_count(), 0);
}

// EnableHpsSense should be called on restart if HpsSense is enabled currently.
TEST_F(HpsSenseControllerTest, EnableHpsSenseOnRestartIfHpsSenseWasEnabled) {
  hps_client_->set_hps_service_is_available(true);
  auto hps_sense_controller = std::make_unique<HpsSenseController>();
  base::RunLoop().RunUntilIdle();
  hps_sense_controller->EnableHpsSense();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(hps_client_->disable_hps_sense_count(), 1);
  EXPECT_EQ(hps_client_->enable_hps_sense_count(), 1);

  // HpsSense is enabled; Restart will call EnableHpsSense.
  hps_client_->Restart();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(hps_client_->disable_hps_sense_count(), 1);
  EXPECT_EQ(hps_client_->enable_hps_sense_count(), 2);
}

// DisableHpsSense should be called on service available even if we need to
// enable it immediately after that.
TEST_F(HpsSenseControllerTest, AlwaysCallDisableOnServiceAvailable) {
  hps_client_->set_hps_service_is_available(true);
  auto hps_sense_controller = std::make_unique<HpsSenseController>();
  hps_sense_controller->EnableHpsSense();
  // At this point, OnServiceAvailable is not called yet, so no disable/enable
  // functions should be called.
  EXPECT_EQ(hps_client_->disable_hps_sense_count(), 0);
  EXPECT_EQ(hps_client_->enable_hps_sense_count(), 0);
  base::RunLoop().RunUntilIdle();
  // Although we only need enabling, the disable function should also be called.
  EXPECT_EQ(hps_client_->disable_hps_sense_count(), 1);
  EXPECT_EQ(hps_client_->enable_hps_sense_count(), 1);
}

}  // namespace ash
