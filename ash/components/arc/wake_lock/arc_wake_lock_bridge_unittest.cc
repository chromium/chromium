// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/wake_lock/arc_wake_lock_bridge.h"

#include <memory>
#include <utility>

#include "ash/components/arc/mojom/power.mojom.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "ash/components/arc/test/fake_power_instance.h"
#include "ash/components/arc/test/fake_wake_lock_instance.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "services/device/public/cpp/test/test_wake_lock_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

using device::mojom::WakeLockType;

class ArcWakeLockBridgeTest : public testing::Test {
 public:
  ArcWakeLockBridgeTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    bridge_service_ = std::make_unique<ArcBridgeService>();
    wake_lock_bridge_ =
        std::make_unique<ArcWakeLockBridge>(nullptr, bridge_service_.get());

    mojo::Remote<device::mojom::WakeLockProvider> remote_provider;
    wake_lock_provider_.BindReceiver(
        remote_provider.BindNewPipeAndPassReceiver());
    wake_lock_bridge_->SetWakeLockProviderForTesting(
        std::move(remote_provider));
    CreateWakeLockInstance();
  }

  ArcWakeLockBridgeTest(const ArcWakeLockBridgeTest&) = delete;
  ArcWakeLockBridgeTest& operator=(const ArcWakeLockBridgeTest&) = delete;

  ~ArcWakeLockBridgeTest() override { DestroyWakeLockInstance(); }

 protected:
  // Returns true iff there is no failure acquiring a system wake lock.
  bool AcquirePartialWakeLock() {
    base::RunLoop loop;
    bool result = false;
    wake_lock_bridge_->AcquirePartialWakeLock(base::BindOnce(
        [](bool* result_out, bool result) { *result_out = result; }, &result));
    loop.RunUntilIdle();
    wake_lock_bridge_->FlushWakeLocksForTesting();
    return result;
  }

  // Returns true iff there is no failure releasing a system wake lock.
  bool ReleasePartialWakeLock() {
    base::RunLoop loop;
    bool result = false;
    wake_lock_bridge_->ReleasePartialWakeLock(base::BindOnce(
        [](bool* result_out, bool result) { *result_out = result; }, &result));
    loop.RunUntilIdle();
    wake_lock_bridge_->FlushWakeLocksForTesting();
    return result;
  }

  // Creates a FakeWakeLockInstance for |bridge_service_|. This results in
  // ArcWakeLockBridge::OnInstanceReady() being called.
  void CreateWakeLockInstance() {
    instance_ = std::make_unique<FakeWakeLockInstance>();
    bridge_service_->wake_lock()->SetInstance(instance_.get());
    WaitForInstanceReady(bridge_service_->wake_lock());
  }

  // Destroys the FakeWakeLockInstance. This results in
  // ArcWakeLockBridge::OnInstanceClosed() being called.
  void DestroyWakeLockInstance() {
    if (!instance_)
      return;
    bridge_service_->wake_lock()->CloseInstance(instance_.get());
    instance_.reset();
  }

  // Returns the number of active wake locks of type |type|.
  int GetActiveWakeLocks(WakeLockType type) {
    base::RunLoop run_loop;
    int result_count = 0;
    wake_lock_provider_.GetActiveWakeLocksForTests(
        type,
        base::BindOnce(
            [](base::RunLoop* run_loop, int* result_count, int32_t count) {
              *result_count = count;
              run_loop->Quit();
            },
            &run_loop, &result_count));
    run_loop.Run();
    return result_count;
  }

 private:
  base::test::TaskEnvironment task_environment_;

  device::TestWakeLockProvider wake_lock_provider_;

  std::unique_ptr<ArcBridgeService> bridge_service_;
  std::unique_ptr<FakeWakeLockInstance> instance_;
  std::unique_ptr<ArcWakeLockBridge> wake_lock_bridge_;
};

TEST_F(ArcWakeLockBridgeTest, AcquireAndReleaseSinglePartialWakeLock) {
  EXPECT_TRUE(AcquirePartialWakeLock());
  EXPECT_EQ(1, GetActiveWakeLocks(WakeLockType::kPreventAppSuspension));

  EXPECT_TRUE(ReleasePartialWakeLock());
  EXPECT_EQ(0, GetActiveWakeLocks(WakeLockType::kPreventAppSuspension));
}

TEST_F(ArcWakeLockBridgeTest, AcquireAndReleaseMultiplePartialWakeLocks) {
  // Taking multiple wake locks should result in only one active wake lock.
  EXPECT_TRUE(AcquirePartialWakeLock());
  EXPECT_TRUE(AcquirePartialWakeLock());
  EXPECT_TRUE(AcquirePartialWakeLock());
  EXPECT_EQ(1, GetActiveWakeLocks(WakeLockType::kPreventAppSuspension));

  // Releasing two wake locks after acquiring three should not result in
  // releasing a wake lock.
  EXPECT_TRUE(ReleasePartialWakeLock());
  EXPECT_TRUE(ReleasePartialWakeLock());
  EXPECT_EQ(1, GetActiveWakeLocks(WakeLockType::kPreventAppSuspension));

  // Releasing the remaining wake lock should result in the release of the wake
  // lock.
  EXPECT_TRUE(ReleasePartialWakeLock());
  EXPECT_EQ(0, GetActiveWakeLocks(WakeLockType::kPreventAppSuspension));
}

TEST_F(ArcWakeLockBridgeTest, ReleaseWakeLockOnInstanceClosed) {
  EXPECT_TRUE(AcquirePartialWakeLock());
  ASSERT_EQ(1, GetActiveWakeLocks(WakeLockType::kPreventAppSuspension));

  // If the instance is closed, all wake locks should be released.
  base::RunLoop run_loop;
  DestroyWakeLockInstance();
  run_loop.RunUntilIdle();
  EXPECT_EQ(0, GetActiveWakeLocks(WakeLockType::kPreventDisplaySleep));

  // Check that wake locks can be requested after the instance becomes ready
  // again.
  CreateWakeLockInstance();
  EXPECT_TRUE(AcquirePartialWakeLock());
  EXPECT_EQ(1, GetActiveWakeLocks(WakeLockType::kPreventAppSuspension));
}

}  // namespace arc
