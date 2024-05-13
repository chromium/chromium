// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/cec_private_ash.h"

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/ash/components/dbus/cec_service/cec_service_client.h"
#include "chromeos/ash/components/dbus/cec_service/fake_cec_service_client.h"
#include "chromeos/crosapi/mojom/cec_private.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crosapi {

using ::base::test::TestFuture;
using DisplayPowerStates = const std::vector<mojom::PowerState>&;

class CecPrivateAshTest : public ::testing::Test {
 public:
  void SetUp() override {
    if (ash::CecServiceClient::Get() == nullptr) {
      ash::CecServiceClient::InitializeFake();
    }
    cec_fake_ =
        static_cast<ash::FakeCecServiceClient*>(ash::CecServiceClient::Get());
    CecPrivateAshTest::cec_fake_->reset();
  }

 protected:
  static raw_ptr<ash::FakeCecServiceClient> cec_fake_;
  CecPrivateAsh cec_private_ash_;
};

raw_ptr<ash::FakeCecServiceClient> CecPrivateAshTest::cec_fake_ = nullptr;

TEST_F(CecPrivateAshTest, SendStandBy) {
  ASSERT_EQ(0, CecPrivateAshTest::cec_fake_->stand_by_call_count());

  TestFuture<void> cb;
  cec_private_ash_.SendStandBy(cb.GetCallback());
  EXPECT_TRUE(cb.Wait());

  ASSERT_EQ(1, CecPrivateAshTest::cec_fake_->stand_by_call_count());
}

TEST_F(CecPrivateAshTest, SendWakeUp) {
  ASSERT_EQ(0, CecPrivateAshTest::cec_fake_->wake_up_call_count());

  TestFuture<void> cb;
  cec_private_ash_.SendWakeUp(cb.GetCallback());
  EXPECT_TRUE(cb.Wait());

  ASSERT_EQ(1, CecPrivateAshTest::cec_fake_->wake_up_call_count());
}

TEST_F(CecPrivateAshTest, QueryPowerStateSeveralDevices) {
  // The fake dbus cec client demands that the query runs in a
  // SingleThreadTaskRunner
  base::test::SingleThreadTaskEnvironment task_environment;

  CecPrivateAshTest::cec_fake_->set_tv_power_states(
      {ash::CecServiceClient::PowerState::kOn,
       ash::CecServiceClient::PowerState::kStandBy,
       ash::CecServiceClient::PowerState::kTransitioningToStandBy});

  TestFuture<DisplayPowerStates> cb;
  cec_private_ash_.QueryDisplayCecPowerState(cb.GetCallback());
  EXPECT_EQ(cb.Get(), std::vector<mojom::PowerState>(
                          {mojom::PowerState::kOn, mojom::PowerState::kStandBy,
                           mojom::PowerState::kTransitioningToStandBy}));
}

TEST_F(CecPrivateAshTest, QueryPowerStateNoDevices) {
  // The fake dbus cec client demands that the query runs in a
  // SingleThreadTaskRunner
  base::test::SingleThreadTaskEnvironment task_environment;

  TestFuture<DisplayPowerStates> cb;
  cec_private_ash_.QueryDisplayCecPowerState(cb.GetCallback());
  EXPECT_EQ(0u, cb.Get().size());
}

TEST_F(CecPrivateAshTest, StandByPropagates) {
  // The fake dbus cec client demands that the query runs in a
  // SingleThreadTaskRunner
  base::test::SingleThreadTaskEnvironment task_environment;

  CecPrivateAshTest::cec_fake_->set_tv_power_states(
      {ash::CecServiceClient::PowerState::kOn,
       ash::CecServiceClient::PowerState::kStandBy,
       ash::CecServiceClient::PowerState::kTransitioningToStandBy});
  TestFuture<void> standby_cb;
  cec_private_ash_.SendStandBy(standby_cb.GetCallback());
  ASSERT_TRUE(standby_cb.Wait());

  TestFuture<DisplayPowerStates> query_cb;
  cec_private_ash_.QueryDisplayCecPowerState(query_cb.GetCallback());
  EXPECT_EQ(query_cb.Get(),
            std::vector<mojom::PowerState>({mojom::PowerState::kStandBy,
                                            mojom::PowerState::kStandBy,
                                            mojom::PowerState::kStandBy}));
}

TEST_F(CecPrivateAshTest, WakePropagates) {
  // The fake dbus cec client demands that the query runs in a
  // SingleThreadTaskRunner
  base::test::SingleThreadTaskEnvironment task_environment;

  CecPrivateAshTest::cec_fake_->set_tv_power_states(
      {ash::CecServiceClient::PowerState::kOn,
       ash::CecServiceClient::PowerState::kStandBy,
       ash::CecServiceClient::PowerState::kTransitioningToStandBy});
  TestFuture<void> wake_cb;
  cec_private_ash_.SendWakeUp(wake_cb.GetCallback());
  ASSERT_TRUE(wake_cb.Wait());

  TestFuture<DisplayPowerStates> query_cb;
  cec_private_ash_.QueryDisplayCecPowerState(query_cb.GetCallback());
  EXPECT_EQ(query_cb.Get(), std::vector<mojom::PowerState>(
                                {mojom::PowerState::kOn, mojom::PowerState::kOn,
                                 mojom::PowerState::kOn}));
}

TEST_F(CecPrivateAshTest, HandleUninitializedDbusClient) {
  cec_fake_ = nullptr;
  ash::CecServiceClient::Shutdown();
  ASSERT_EQ(ash::CecServiceClient::Get(), nullptr);

  // All these cec_private_ash_ calls are now working with a null dbus client.
  TestFuture<void> standby_cb;
  cec_private_ash_.SendStandBy(standby_cb.GetCallback());
  TestFuture<void> wakeup_cb;
  cec_private_ash_.SendWakeUp(wakeup_cb.GetCallback());
  TestFuture<DisplayPowerStates> query_cb;
  cec_private_ash_.QueryDisplayCecPowerState(query_cb.GetCallback());
  EXPECT_EQ(query_cb.Get(), std::vector<mojom::PowerState>());
}

}  // namespace crosapi
