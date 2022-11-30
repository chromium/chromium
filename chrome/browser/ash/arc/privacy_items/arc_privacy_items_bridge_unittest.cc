// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/privacy_items/arc_privacy_items_bridge.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "ash/components/arc/test/fake_privacy_items_instance.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

class ArcPrivacyItemsBridgeTest : public testing::Test {
 public:
  ArcPrivacyItemsBridgeTest() = default;
  ~ArcPrivacyItemsBridgeTest() override = default;

  void SetUp() override {
    arc_bridge_service_ = std::make_unique<ArcBridgeService>();
    bridge_ = std::make_unique<ArcPrivacyItemsBridge>(
        &testing_profile_, arc_bridge_service_.get());
    privacy_items_instance_ = std::make_unique<arc::FakePrivacyItemsInstance>();
    arc_bridge_service_->privacy_items()->SetInstance(
        privacy_items_instance_.get());
    WaitForInstanceReady(arc_bridge_service_->privacy_items());
  }

  void TearDown() override {
    arc_bridge_service_->privacy_items()->CloseInstance(
        privacy_items_instance_.get());
    privacy_items_instance_.reset();
    bridge_.reset();
    arc_bridge_service_.reset();
  }

  ArcPrivacyItemsBridge* bridge() { return bridge_.get(); }
  FakePrivacyItemsInstance* privacy_items_instance() {
    return privacy_items_instance_.get();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile testing_profile_;
  std::unique_ptr<ArcBridgeService> arc_bridge_service_;
  std::unique_ptr<ArcPrivacyItemsBridge> bridge_;
  std::unique_ptr<FakePrivacyItemsInstance> privacy_items_instance_;
};

TEST_F(ArcPrivacyItemsBridgeTest, ChangingPrivacyBoundsForwardsToAndroid) {
  std::vector<gfx::Rect> bounds;
  bridge()->OnStaticPrivacyIndicatorBoundsChanged(5, bounds);
  EXPECT_EQ(5, privacy_items_instance()->last_bounds_display_id());
  EXPECT_EQ(bounds, privacy_items_instance()->last_bounds());
}

}  // namespace arc
