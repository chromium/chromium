// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/memory/arc_memory_bridge.h"

#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "ash/components/arc/test/fake_memory_instance.h"
#include "ash/components/arc/test/test_browser_context.h"
#include "base/test/bind.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace arc {
namespace {

class ArcMemoryBridgeTest : public testing::Test {
 protected:
  ArcMemoryBridgeTest() = default;
  ArcMemoryBridgeTest(const ArcMemoryBridgeTest&) = delete;
  ArcMemoryBridgeTest& operator=(const ArcMemoryBridgeTest&) = delete;
  ~ArcMemoryBridgeTest() override = default;

  void SetUp() override {
    bridge_ = ArcMemoryBridge::GetForBrowserContextForTesting(&context_);
    // This results in ArcMemoryBridge::OnInstanceReady being called.
    ArcServiceManager::Get()->arc_bridge_service()->memory()->SetInstance(
        &memory_instance_);
    WaitForInstanceReady(
        ArcServiceManager::Get()->arc_bridge_service()->memory());
  }

  ArcMemoryBridge* bridge() { return bridge_; }
  FakeMemoryInstance* memory_instance() { return &memory_instance_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  ArcServiceManager arc_service_manager_;
  FakeMemoryInstance memory_instance_;
  TestBrowserContext context_;
  ArcMemoryBridge* bridge_ = nullptr;
};

TEST_F(ArcMemoryBridgeTest, ConstructDestruct) {
  EXPECT_NE(nullptr, bridge());
}

// Tests that DropCaches runs the callback passed.
TEST_F(ArcMemoryBridgeTest, DropCaches) {
  absl::optional<bool> opt_result;
  bridge()->DropCaches(base::BindLambdaForTesting(
      [&opt_result](bool result) { opt_result = result; }));
  ASSERT_TRUE(opt_result);
  EXPECT_TRUE(*opt_result);
}

// Tests that DropCaches runs the callback with a proper result.
TEST_F(ArcMemoryBridgeTest, DropCaches_Fail) {
  // Inject failure.
  memory_instance()->set_drop_caches_result(false);

  absl::optional<bool> opt_result;
  bridge()->DropCaches(base::BindLambdaForTesting(
      [&opt_result](bool result) { opt_result = result; }));
  ASSERT_TRUE(opt_result);
  EXPECT_FALSE(*opt_result);
}

// Tests that DropCaches runs the callback with a proper result.
TEST_F(ArcMemoryBridgeTest, DropCaches_NoInstance) {
  // Inject failure.
  ArcServiceManager::Get()->arc_bridge_service()->memory()->CloseInstance(
      memory_instance());

  absl::optional<bool> opt_result;
  bridge()->DropCaches(base::BindLambdaForTesting(
      [&opt_result](bool result) { opt_result = result; }));
  ASSERT_TRUE(opt_result);
  EXPECT_FALSE(*opt_result);
}

// Tests that ReclaimAll runs the callback with memory reclaimed from all
// processes successfully.
TEST_F(ArcMemoryBridgeTest, ReclaimAll_All_Success) {
  memory_instance()->set_reclaim_all_result(100, 0);

  absl::optional<uint32_t> reclaimed_result;
  absl::optional<uint32_t> unreclaimed_result;
  bridge()->ReclaimAll(
      base::BindLambdaForTesting([&](uint32_t reclaimed, uint32_t unreclaimed) {
        reclaimed_result = reclaimed;
        unreclaimed_result = unreclaimed;
      }));

  ASSERT_TRUE(reclaimed_result);
  EXPECT_EQ(*reclaimed_result, 100u);
  ASSERT_TRUE(unreclaimed_result);
  EXPECT_EQ(*unreclaimed_result, 0u);
}

// Tests that ReclaimAll runs the callback with memory reclaimed from some
// processes successfully.
TEST_F(ArcMemoryBridgeTest, ReclaimAll_Partial_Success) {
  memory_instance()->set_reclaim_all_result(50, 50);

  absl::optional<uint32_t> reclaimed_result;
  absl::optional<uint32_t> unreclaimed_result;
  bridge()->ReclaimAll(
      base::BindLambdaForTesting([&](uint32_t reclaimed, uint32_t unreclaimed) {
        reclaimed_result = reclaimed;
        unreclaimed_result = unreclaimed;
      }));

  ASSERT_TRUE(reclaimed_result);
  EXPECT_EQ(*reclaimed_result, 50u);
  ASSERT_TRUE(unreclaimed_result);
  EXPECT_EQ(*unreclaimed_result, 50u);
}

// Tests that ReclaimAll runs the callback with the instance not available.
TEST_F(ArcMemoryBridgeTest, ReclaimAll_NoInstance) {
  // Inject failure.
  ArcServiceManager::Get()->arc_bridge_service()->memory()->CloseInstance(
      memory_instance());

  absl::optional<uint32_t> reclaimed_result;
  absl::optional<uint32_t> unreclaimed_result;
  bridge()->ReclaimAll(
      base::BindLambdaForTesting([&](uint32_t reclaimed, uint32_t unreclaimed) {
        reclaimed_result = reclaimed;
        unreclaimed_result = unreclaimed;
      }));

  ASSERT_TRUE(reclaimed_result);
  EXPECT_EQ(*reclaimed_result, 0u);
  ASSERT_TRUE(unreclaimed_result);
  EXPECT_EQ(*unreclaimed_result, 0u);
}

}  // namespace
}  // namespace arc
