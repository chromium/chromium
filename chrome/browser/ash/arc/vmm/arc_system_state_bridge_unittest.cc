// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/vmm/arc_system_state_bridge.h"

#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "ash/components/arc/test/fake_system_state_instance.h"
#include "base/memory/raw_ptr.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

class ArcSystemStateBridgeTest : public testing::Test {
 public:
  ArcSystemStateBridgeTest() = default;
  ArcSystemStateBridgeTest(const ArcSystemStateBridgeTest&) = delete;
  ArcSystemStateBridgeTest& operator=(const ArcSystemStateBridgeTest&) = delete;
  ~ArcSystemStateBridgeTest() override = default;

  void SetUp() override {
    bridge_ = ArcSystemStateBridge::GetForBrowserContextForTesting(&profile_);

    EXPECT_EQ(0u, system_state_instance()->num_init_called());

    // Connect to instance.
    ArcServiceManager::Get()->arc_bridge_service()->system_state()->SetInstance(
        &system_state_instance_);
    WaitForInstanceReady(
        ArcServiceManager::Get()->arc_bridge_service()->system_state());
    EXPECT_EQ(1u, system_state_instance()->num_init_called());
  }

  ArcSystemStateBridge* bridge() { return bridge_; }
  const FakeSystemStateInstance* system_state_instance() const {
    return &system_state_instance_;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  ArcServiceManager arc_service_manager_;
  FakeSystemStateInstance system_state_instance_;
  TestingProfile profile_;
  raw_ptr<ArcSystemStateBridge> bridge_ = nullptr;
};

TEST_F(ArcSystemStateBridgeTest, ConstructDestruct) {
  EXPECT_NE(nullptr, bridge());
}

}  // namespace arc
