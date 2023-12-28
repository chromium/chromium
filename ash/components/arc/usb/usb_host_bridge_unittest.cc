// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/usb/usb_host_bridge.h"

#include "ash/components/arc/session/arc_service_manager.h"
#include "base/memory/raw_ptr.h"
#include "components/user_prefs/test/test_browser_context_with_prefs.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace {

class ArcUsbHostBridgeTest : public testing::Test {
 protected:
  ArcUsbHostBridgeTest()
      : bridge_(ArcUsbHostBridge::GetForBrowserContextForTesting(&context_)) {}
  ArcUsbHostBridgeTest(const ArcUsbHostBridgeTest&) = delete;
  ArcUsbHostBridgeTest& operator=(const ArcUsbHostBridgeTest&) = delete;
  ~ArcUsbHostBridgeTest() override = default;

  ArcUsbHostBridge* bridge() { return bridge_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  ArcServiceManager arc_service_manager_;
  user_prefs::TestBrowserContextWithPrefs context_;
  const raw_ptr<ArcUsbHostBridge> bridge_;
};

TEST_F(ArcUsbHostBridgeTest, ConstructDestruct) {
  EXPECT_NE(nullptr, bridge());
}

}  // namespace
}  // namespace arc
