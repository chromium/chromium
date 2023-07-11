// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/disk_quota/arc_disk_quota_bridge.h"

#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/test_browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace {

class ArcDiskQuotaBridgeTest : public testing::Test {
 protected:
  ArcDiskQuotaBridgeTest()
      : bridge_(ArcDiskQuotaBridge::GetForBrowserContextForTesting(&context_)) {
  }
  ArcDiskQuotaBridgeTest(const ArcDiskQuotaBridgeTest&) = delete;
  ArcDiskQuotaBridgeTest& operator=(const ArcDiskQuotaBridgeTest&) = delete;
  ~ArcDiskQuotaBridgeTest() override = default;

  ArcDiskQuotaBridge* bridge() { return bridge_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  ArcServiceManager arc_service_manager_;
  TestBrowserContext context_;
  const raw_ptr<ArcDiskQuotaBridge, ExperimentalAsh> bridge_;
};

TEST_F(ArcDiskQuotaBridgeTest, ConstructDistruct) {}

// TODO(b/229122701): Add more test cases after migrating ArcQuota from
// cryptohome to spaced.

}  // namespace
}  // namespace arc
