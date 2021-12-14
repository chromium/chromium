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
    hps_sense_controller_ = std::make_unique<HpsSenseController>();
  }

 protected:
  chromeos::FakeHpsDBusClient* hps_client_ = nullptr;
  std::unique_ptr<HpsSenseController> hps_sense_controller_;

  base::test::SingleThreadTaskEnvironment task_environment_;
};

// EnableHpsSense should be skipped if HpsService is not available.
TEST_F(HpsSenseControllerTest,
       EnableHpsSenseDoesNothingIfHpsServiceUnavailable) {
  hps_sense_controller_->EnableHpsSense();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(hps_client_->enable_hps_sense_count(), 0);
}

// EnableHpsSense should succeed if HpsService is available.
TEST_F(HpsSenseControllerTest, EnableHpsSenseSucceedsIfHpsServiceAvailable) {
  hps_client_->set_hps_service_is_available(true);
  hps_sense_controller_->EnableHpsSense();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(hps_client_->enable_hps_sense_count(), 1);
}

// EnableHpsSense should only be applied once for multiple calls of
// EnableHpsSense.
TEST_F(HpsSenseControllerTest, EnableHpsSenseOnlyCalledOnceOnTwoCalls) {
  hps_client_->set_hps_service_is_available(true);
  hps_sense_controller_->EnableHpsSense();
  base::RunLoop().RunUntilIdle();
  hps_sense_controller_->EnableHpsSense();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(hps_client_->enable_hps_sense_count(), 1);
}

// DisableHpsSense should only be applied if HpsSense is enabled currently.
TEST_F(HpsSenseControllerTest, DisableHpsSense) {
  hps_client_->set_hps_service_is_available(true);
  // DisableHpsSense does nothing if HpsSense is not enabled yet.
  hps_sense_controller_->DisableHpsSense();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(hps_client_->disable_hps_sense_count(), 0);

  // DisableHpsSense succeeds if HpsSense is enabled.
  hps_sense_controller_->EnableHpsSense();
  base::RunLoop().RunUntilIdle();
  hps_sense_controller_->DisableHpsSense();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(hps_client_->enable_hps_sense_count(), 1);
  EXPECT_EQ(hps_client_->disable_hps_sense_count(), 1);

  // Call DisableHpsSense again does nothing since HpsSense is not enabled.
  hps_sense_controller_->DisableHpsSense();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(hps_client_->disable_hps_sense_count(), 1);
}

// EnableHpsSense should be called on restart if HpsSense is enabled currently.
TEST_F(HpsSenseControllerTest, OnRestart) {
  hps_client_->set_hps_service_is_available(true);

  // HpsSense is not enabled; Restart will not cause any action.
  hps_client_->Restart();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(hps_client_->enable_hps_sense_count(), 0);
  EXPECT_EQ(hps_client_->disable_hps_sense_count(), 0);

  hps_sense_controller_->EnableHpsSense();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(hps_client_->enable_hps_sense_count(), 1);
  EXPECT_EQ(hps_client_->disable_hps_sense_count(), 0);

  // HpsSense is enabled; Restart will call EnableHpsSense.
  hps_client_->Restart();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(hps_client_->enable_hps_sense_count(), 2);
  EXPECT_EQ(hps_client_->disable_hps_sense_count(), 0);
}

}  // namespace ash
