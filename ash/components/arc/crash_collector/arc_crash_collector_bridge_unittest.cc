// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/crash_collector/arc_crash_collector_bridge.h"

#include <unistd.h>

#include "ash/components/arc/session/arc_service_manager.h"
#include "base/memory/raw_ptr.h"
#include "components/user_prefs/test/test_browser_context_with_prefs.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace {

class ArcCrashCollectorBridgeTest : public testing::Test {
 protected:
  ArcCrashCollectorBridgeTest()
      : bridge_(ArcCrashCollectorBridge::GetForBrowserContextForTesting(
            &context_)) {}
  ArcCrashCollectorBridgeTest(const ArcCrashCollectorBridgeTest&) = delete;
  ArcCrashCollectorBridgeTest& operator=(const ArcCrashCollectorBridgeTest&) =
      delete;
  ~ArcCrashCollectorBridgeTest() override = default;

  ArcCrashCollectorBridge* bridge() { return bridge_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  ArcServiceManager arc_service_manager_;
  user_prefs::TestBrowserContextWithPrefs context_;
  const raw_ptr<ArcCrashCollectorBridge> bridge_;
};

TEST_F(ArcCrashCollectorBridgeTest, ConstructDestruct) {}

// Tests that SetBuildProperties doesn't crash even if nullopt is passed as a
// fingerprint.
TEST_F(ArcCrashCollectorBridgeTest, SetBuildProperties) {
  ASSERT_NE(nullptr, bridge());
  bridge()->SetBuildProperties("device", "board", "cpu_abi",
                               std::optional<std::string>());
  bridge()->SetBuildProperties("device", "board", "cpu_abi",
                               std::optional<std::string>("fingerprint"));
}

// Tests that DumpCrash doesn't crash.
// TODO(khmel): Test the behavior beyond just "no crash".
TEST_F(ArcCrashCollectorBridgeTest, DumpCrash) {
  ASSERT_NE(nullptr, bridge());
  bridge()->SetBuildProperties("device", "board", "cpu_abi",
                               std::optional<std::string>());
  bridge()->DumpCrash("type", mojo::ScopedHandle(), std::nullopt);
}

// Tests that DumpNativeCrash doesn't crash.
// TODO(khmel): Test the behavior beyond just "no crash".
TEST_F(ArcCrashCollectorBridgeTest, DumpNativeCrash) {
  ASSERT_NE(nullptr, bridge());
  bridge()->SetBuildProperties("device", "board", "cpu_abi",
                               std::optional<std::string>());
  bridge()->DumpNativeCrash("exec_name", getpid(), /*timestamp=*/42,
                            mojo::ScopedHandle());
}

// Tests that DumpKernelCrash doesn't crash.
// TODO(khmel): Test the behavior beyond just "no crash".
TEST_F(ArcCrashCollectorBridgeTest, DumpKernelCrash) {
  ASSERT_NE(nullptr, bridge());
  bridge()->SetBuildProperties("device", "board", "cpu_abi",
                               std::optional<std::string>());
  bridge()->DumpKernelCrash(mojo::ScopedHandle());
}

}  // namespace
}  // namespace arc
