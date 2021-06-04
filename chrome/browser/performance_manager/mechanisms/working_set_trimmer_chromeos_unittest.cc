// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/mechanisms/working_set_trimmer_chromeos.h"

#include <memory>
#include <string>
#include <utility>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/arc/test/test_arc_session_manager.h"
#include "chromeos/dbus/concierge/concierge_client.h"
#include "components/arc/test/fake_arc_session.h"
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
  }

  void TearDown() override {
    TearDownArcSessionManager();
    chromeos::ConciergeClient::Shutdown();
  }

 protected:
  void TrimArcVmWorkingSet(
      WorkingSetTrimmerChromeOS::TrimArcVmWorkingSetCallback callback) {
    trimmer_.TrimArcVmWorkingSet(std::move(callback));
  }

  void TearDownArcSessionManager() { arc_session_manager_.reset(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<arc::ArcSessionManager> arc_session_manager_;
  WorkingSetTrimmerChromeOS trimmer_;
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
// ArcSessionManager is not available.
TEST_F(TestWorkingSetTrimmerChromeOS, TrimArcVmWorkingSetNoArcSessionManager) {
  absl::optional<bool> result;
  TearDownArcSessionManager();
  TrimArcVmWorkingSet(base::BindLambdaForTesting(
      [&result](bool r, const std::string&) { result = r; }));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(result);
  EXPECT_FALSE(*result);
}

}  // namespace
}  // namespace mechanism
}  // namespace performance_manager
