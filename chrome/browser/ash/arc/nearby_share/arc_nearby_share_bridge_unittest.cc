// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/nearby_share/arc_nearby_share_bridge.h"

#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "ash/components/arc/test/fake_nearby_share_instance.h"
#include "base/memory/raw_ptr.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

class ArcNearbyShareBridgeTest : public testing::Test {
 public:
  ArcNearbyShareBridgeTest() = default;
  ~ArcNearbyShareBridgeTest() override = default;

  ArcNearbyShareBridgeTest(const ArcNearbyShareBridgeTest&) = delete;
  ArcNearbyShareBridgeTest& operator=(const ArcNearbyShareBridgeTest&) = delete;

  void SetUp() override {
    bridge_ = ArcNearbyShareBridge::GetForBrowserContextForTesting(&profile_);

    EXPECT_EQ(0u, nearby_share_instance_.num_init_called());
    ArcServiceManager::Get()->arc_bridge_service()->nearby_share()->SetInstance(
        &nearby_share_instance_);
    WaitForInstanceReady(
        ArcServiceManager::Get()->arc_bridge_service()->nearby_share());
    // Tests that FakeNearbyShareInstance's Init() method is called after the
    // instance connects to the host.
    EXPECT_EQ(1u, nearby_share_instance_.num_init_called());
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  ArcServiceManager arc_service_manager_;
  FakeNearbyShareInstance nearby_share_instance_;
  TestingProfile profile_;
  raw_ptr<ArcNearbyShareBridge> bridge_ = nullptr;
};

TEST_F(ArcNearbyShareBridgeTest, ConstructDestruct) {}

}  // namespace arc
