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

}  // namespace
}  // namespace arc
