// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/media_session/arc_media_session_bridge.h"

#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/test_browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace {

class ArcMediaSessionBridgeTest : public testing::Test {
 protected:
  ArcMediaSessionBridgeTest()
      : bridge_(
            ArcMediaSessionBridge::GetForBrowserContextForTesting(&context_)) {}
  ArcMediaSessionBridgeTest(const ArcMediaSessionBridgeTest&) = delete;
  ArcMediaSessionBridgeTest& operator=(const ArcMediaSessionBridgeTest&) =
      delete;
  ~ArcMediaSessionBridgeTest() override = default;

  ArcMediaSessionBridge* bridge() { return bridge_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  ArcServiceManager arc_service_manager_;
  TestBrowserContext context_;
  ArcMediaSessionBridge* const bridge_;
};

TEST_F(ArcMediaSessionBridgeTest, ConstructDestruct) {
  EXPECT_NE(nullptr, bridge());
}

}  // namespace
}  // namespace arc
