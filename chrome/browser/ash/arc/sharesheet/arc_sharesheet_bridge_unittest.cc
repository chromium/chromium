// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/sharesheet/arc_sharesheet_bridge.h"

#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "ash/components/arc/test/fake_sharesheet_instance.h"
#include "base/memory/raw_ptr.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace {

class ArcSharesheetBridgeTest : public testing::Test {
 protected:
  ArcSharesheetBridgeTest() = default;
  ArcSharesheetBridgeTest(const ArcSharesheetBridgeTest&) = delete;
  ArcSharesheetBridgeTest& operator=(const ArcSharesheetBridgeTest&) = delete;
  ~ArcSharesheetBridgeTest() override = default;

  void SetUp() override {
    bridge_ = ArcSharesheetBridge::GetForBrowserContextForTesting(&profile_);

    EXPECT_EQ(0u, sharesheet_instance()->num_init_called());
    // This results in ArcSharesheetBridge::OnInstanceReady being called.
    ArcServiceManager::Get()->arc_bridge_service()->sharesheet()->SetInstance(
        &sharesheet_instance_);
    WaitForInstanceReady(
        ArcServiceManager::Get()->arc_bridge_service()->sharesheet());
    // Tests that SharesheetInstance's Init() method is called after the
    // instance connects to the host.
    EXPECT_EQ(1u, sharesheet_instance()->num_init_called());
  }

  ArcSharesheetBridge* bridge() { return bridge_; }
  const FakeSharesheetInstance* sharesheet_instance() const {
    return &sharesheet_instance_;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  ArcServiceManager arc_service_manager_;
  FakeSharesheetInstance sharesheet_instance_;
  TestingProfile profile_;
  raw_ptr<ArcSharesheetBridge> bridge_ = nullptr;
};

TEST_F(ArcSharesheetBridgeTest, ConstructDestruct) {
  EXPECT_NE(nullptr, bridge());
}

}  // namespace
}  // namespace arc
