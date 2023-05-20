// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/vmm/arcvm_working_set_trim_executor.h"

#include "ash/components/arc/memory/arc_memory_bridge.h"
#include "ash/components/arc/mojom/memory.mojom.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/timer/timer.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {

const base::TimeDelta DELAYED_TIME_DELTA = base::Seconds(10);

class DelayedMemoryInstance : public mojom::MemoryInstance {
 public:
  DelayedMemoryInstance() = default;
  DelayedMemoryInstance(const DelayedMemoryInstance&) = delete;
  DelayedMemoryInstance& operator=(const DelayedMemoryInstance&) = delete;
  ~DelayedMemoryInstance() override = default;

  // mojom::MemoryInstance:
  void DropCaches(DropCachesCallback callback) override {
    if (timer_.IsRunning()) {
      return;
    }
    timer_.Start(FROM_HERE, DELAYED_TIME_DELTA,
                 base::BindOnce(std::move(callback), true));
  }

  // mojom::MemoryInstance:
  void Reclaim(mojom::ReclaimRequestPtr request,
               ReclaimCallback callback) override {}

 private:
  base::OneShotTimer timer_;
};

}  // namespace

class ArcVmWorkingSetTrimExecutorTest : public testing::Test {
 public:
  ArcVmWorkingSetTrimExecutorTest() = default;

  ArcVmWorkingSetTrimExecutorTest(const ArcVmWorkingSetTrimExecutorTest&) =
      delete;
  ArcVmWorkingSetTrimExecutorTest& operator=(
      const ArcVmWorkingSetTrimExecutorTest&) = delete;

  ~ArcVmWorkingSetTrimExecutorTest() override = default;

  void SetUp() override {
    arc_service_manager_ = std::make_unique<ArcServiceManager>();
    testing_profile_ = std::make_unique<TestingProfile>();
    arc::ArcMemoryBridge::GetForBrowserContextForTesting(
        testing_profile_.get());
    arc::ArcServiceManager::Get()->arc_bridge_service()->memory()->SetInstance(
        &memory_instance_);
    arc::WaitForInstanceReady(
        arc::ArcServiceManager::Get()->arc_bridge_service()->memory());
  }

  void TearDown() override { testing_profile_.reset(); }

  TestingProfile* testing_profile() { return testing_profile_.get(); }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  DelayedMemoryInstance memory_instance_;
  std::unique_ptr<TestingProfile> testing_profile_;
  std::unique_ptr<ArcServiceManager> arc_service_manager_;
};

TEST_F(ArcVmWorkingSetTrimExecutorTest, NoTrimAgainIfLastTrimStillWorking) {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&ArcVmWorkingSetTrimExecutor::Trim, testing_profile(),
                     base::BindOnce([](bool result, const std::string& msg) {
                       // Not failed by double trim.
                       EXPECT_EQ(msg.find("skip"), std::string::npos);
                     }),
                     ArcVmReclaimType::kReclaimAll, 0));
  task_environment_.RunUntilIdle();
  // Double Trim.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&ArcVmWorkingSetTrimExecutor::Trim, testing_profile(),
                     base::BindOnce([](bool result, const std::string& msg) {
                       // Expect failed by double trim.
                       EXPECT_NE(msg.find("skip"), std::string::npos);
                     }),
                     ArcVmReclaimType::kReclaimAll, 0));

  task_environment_.FastForwardBy(DELAYED_TIME_DELTA);
}
}  // namespace arc
