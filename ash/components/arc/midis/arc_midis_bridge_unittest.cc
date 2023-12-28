// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/midis/arc_midis_bridge.h"

#include "ash/components/arc/session/arc_service_manager.h"
#include "base/memory/raw_ptr.h"
#include "components/user_prefs/test/test_browser_context_with_prefs.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace {

class ArcMidisBridgeTest : public testing::Test {
 protected:
  ArcMidisBridgeTest()
      : bridge_(ArcMidisBridge::GetForBrowserContextForTesting(&context_)) {}
  ArcMidisBridgeTest(const ArcMidisBridgeTest&) = delete;
  ArcMidisBridgeTest& operator=(const ArcMidisBridgeTest&) = delete;
  ~ArcMidisBridgeTest() override = default;

  ArcMidisBridge* bridge() { return bridge_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  ArcServiceManager arc_service_manager_;
  user_prefs::TestBrowserContextWithPrefs context_;
  const raw_ptr<ArcMidisBridge> bridge_;
};

TEST_F(ArcMidisBridgeTest, ConstructDestruct) {
  EXPECT_NE(nullptr, bridge());
}

}  // namespace
}  // namespace arc
