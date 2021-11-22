// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/dark_theme/arc_dark_theme_bridge.h"

#include <memory>

#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "ash/components/arc/test/fake_dark_theme_instance.h"
#include "ash/components/arc/test/test_browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace {

class ArcDarkThemeBridgeTest : public testing::Test {
 protected:
  ArcDarkThemeBridgeTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP),
        bridge_(ArcDarkThemeBridge::GetForBrowserContextForTesting(&context_)) {
    ArcServiceManager::Get()->arc_bridge_service()->dark_theme()->SetInstance(
        &dark_theme_instance_);
    WaitForInstanceReady(
        ArcServiceManager::Get()->arc_bridge_service()->dark_theme());
  }

  ~ArcDarkThemeBridgeTest() override {
    ArcServiceManager::Get()->arc_bridge_service()->dark_theme()->CloseInstance(
        &dark_theme_instance_);
    bridge_->Shutdown();
  }

  ArcDarkThemeBridgeTest(const ArcDarkThemeBridge&) = delete;
  ArcDarkThemeBridgeTest& operator=(const ArcDarkThemeBridge&) = delete;

  content::BrowserTaskEnvironment task_environment_;
  ArcServiceManager arc_service_manager_;
  TestBrowserContext context_;
  FakeDarkThemeInstance dark_theme_instance_;
  ArcDarkThemeBridge* const bridge_;
};

TEST_F(ArcDarkThemeBridgeTest, ConstructDestruct) {}

TEST_F(ArcDarkThemeBridgeTest, SendDeviceDarkThemeState) {
  EXPECT_FALSE(dark_theme_instance_.dark_theme_status());
  bridge_->SendDeviceDarkThemeStateForTesting(true);
  EXPECT_TRUE(dark_theme_instance_.dark_theme_status());
  bridge_->SendDeviceDarkThemeStateForTesting(false);
  EXPECT_FALSE(dark_theme_instance_.dark_theme_status());
}

}  // namespace
}  // namespace arc
