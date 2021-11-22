// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/mechanisms/working_set_trimmer_chromeos.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/components/arc/memory/arc_memory_bridge.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/fake_arc_session.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/arc/test/test_arc_session_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/concierge/concierge_client.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace performance_manager {
namespace mechanism {

class TestWorkingSetTrimmerChromeOS : public testing::Test {
 public:
  TestWorkingSetTrimmerChromeOS() = default;
  TestWorkingSetTrimmerChromeOS(const TestWorkingSetTrimmerChromeOS&) = delete;
  TestWorkingSetTrimmerChromeOS& operator=(
      const TestWorkingSetTrimmerChromeOS&) = delete;
  ~TestWorkingSetTrimmerChromeOS() override = default;

  void SetUp() override {
    chromeos::ConciergeClient::InitializeFake(/*fake_cicerone_client=*/nullptr);
    arc_session_manager_ = arc::CreateTestArcSessionManager(
        std::make_unique<arc::ArcSessionRunner>(
            base::BindRepeating(arc::FakeArcSession::Create)));
    testing_profile_ = std::make_unique<TestingProfile>();
    CreateTrimmer(testing_profile_.get());
    arc::ArcMemoryBridge::GetForBrowserContextForTesting(
        testing_profile_.get());
  }

  void TearDown() override {
    trimmer_.reset();
    testing_profile_.reset();
    TearDownArcSessionManager();
    chromeos::ConciergeClient::Shutdown();
  }

 protected:
  void CreateTrimmer(content::BrowserContext* context) {
    trimmer_ = WorkingSetTrimmerChromeOS::CreateForTesting(context);
  }
  void TrimArcVmWorkingSet(
      WorkingSetTrimmerChromeOS::TrimArcVmWorkingSetCallback callback) {
    trimmer_->TrimArcVmWorkingSet(std::move(callback));
  }

  void TearDownArcSessionManager() { arc_session_manager_.reset(); }

  std::unique_ptr<WorkingSetTrimmerChromeOS> trimmer_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  arc::ArcServiceManager arc_service_manager_;
  std::unique_ptr<arc::ArcSessionManager> arc_session_manager_;
  std::unique_ptr<TestingProfile> testing_profile_;
};

namespace {

// Tests that TrimArcVmWorkingSet runs the passed callback.
TEST_F(TestWorkingSetTrimmerChromeOS, TrimArcVmWorkingSet) {
  absl::optional<bool> result;
  TrimArcVmWorkingSet(base::BindLambdaForTesting(
      [&result](bool r, const std::string&) { result = r; }));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(result);
}

// Tests that TrimArcVmWorkingSet runs the passed callback even when
// BrowserContext is not available.
TEST_F(TestWorkingSetTrimmerChromeOS, TrimArcVmWorkingSetNoBrowserContext) {
  // Create a trimmer again with a null BrowserContext to make it unavailable.
  CreateTrimmer(nullptr);

  absl::optional<bool> result;
  TrimArcVmWorkingSet(base::BindLambdaForTesting(
      [&result](bool r, const std::string&) { result = r; }));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(result);
}

// Tests that TrimArcVmWorkingSet runs the passed callback even when
// ArcMemoryBridge is not available.
TEST_F(TestWorkingSetTrimmerChromeOS, TrimArcVmWorkingSetNoArcMemoryBridge) {
  // Create a trimmer again with a different profile (BrowserContext) to make
  // ArcMemoryBridge unavailable.
  TestingProfile another_profile;
  CreateTrimmer(&another_profile);

  absl::optional<bool> result;
  TrimArcVmWorkingSet(base::BindLambdaForTesting(
      [&result](bool r, const std::string&) { result = r; }));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(result);

  trimmer_.reset();
}

// Tests that TrimArcVmWorkingSet runs the passed callback even when
// ArcSessionManager is not available.
TEST_F(TestWorkingSetTrimmerChromeOS, TrimArcVmWorkingSetNoArcSessionManager) {
  // Make ArcSessionManager unavailable.
  TearDownArcSessionManager();

  absl::optional<bool> result;
  TrimArcVmWorkingSet(base::BindLambdaForTesting(
      [&result](bool r, const std::string&) { result = r; }));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(result);
  EXPECT_FALSE(*result);
}

}  // namespace
}  // namespace mechanism
}  // namespace performance_manager
