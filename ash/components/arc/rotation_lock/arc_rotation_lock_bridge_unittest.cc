// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/rotation_lock/arc_rotation_lock_bridge.h"

#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/test_browser_context.h"
#include "base/memory/raw_ptr.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace {

class ArcRotationLockBridgeTest : public testing::Test {
 protected:
  ArcRotationLockBridgeTest()
      : bridge_(
            ArcRotationLockBridge::GetForBrowserContextForTesting(&context_)) {}
  ArcRotationLockBridgeTest(const ArcRotationLockBridgeTest&) = delete;
  ArcRotationLockBridgeTest& operator=(const ArcRotationLockBridgeTest&) =
      delete;
  ~ArcRotationLockBridgeTest() override = default;

  ArcRotationLockBridge* bridge() { return bridge_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  ArcServiceManager arc_service_manager_;
  TestBrowserContext context_;
  const raw_ptr<ArcRotationLockBridge, ExperimentalAsh> bridge_;
};

TEST_F(ArcRotationLockBridgeTest, ConstructDestruct) {
  EXPECT_NE(nullptr, bridge());
}

}  // namespace
}  // namespace arc
