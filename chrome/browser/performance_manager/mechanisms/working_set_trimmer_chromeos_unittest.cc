// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/mechanisms/working_set_trimmer_chromeos.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/components/arc/memory/arc_memory_bridge.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "ash/components/arc/test/fake_arc_session.h"
#include "ash/components/arc/test/fake_memory_instance.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/arc/test/test_arc_session_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
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
    ash::ConciergeClient::InitializeFake(/*fake_cicerone_client=*/nullptr);
    arc_session_manager_ = arc::CreateTestArcSessionManager(
        std::make_unique<arc::ArcSessionRunner>(
            base::BindRepeating(arc::FakeArcSession::Create)));
    testing_profile_ = std::make_unique<TestingProfile>();
    CreateTrimmer(testing_profile_.get());
    arc::ArcMemoryBridge::GetForBrowserContextForTesting(
        testing_profile_.get());

    // Set a fake memory instance so that DropCaches() calls in test will
    // succeed.
    arc::ArcServiceManager::Get()->arc_bridge_service()->memory()->SetInstance(
        &memory_instance_);
    arc::WaitForInstanceReady(
        arc::ArcServiceManager::Get()->arc_bridge_service()->memory());
  }

  void TearDown() override {
    trimmer_.reset();
    testing_profile_.reset();
    TearDownArcSessionManager();
    ash::ConciergeClient::Shutdown();
  }

 protected:
  void CreateTrimmer(content::BrowserContext* context) {
    trimmer_ = WorkingSetTrimmerChromeOS::CreateForTesting(context);
  }
  void TrimArcVmWorkingSet(
      WorkingSetTrimmerChromeOS::TrimArcVmWorkingSetCallback callback) {
    trimmer_->TrimArcVmWorkingSet(std::move(callback),
                                  ArcVmReclaimType::kReclaimAll,
                                  arc::ArcSession::kNoPageLimit);
  }
  void TrimArcVmWorkingSetDropPageCachesOnly(
      WorkingSetTrimmerChromeOS::TrimArcVmWorkingSetCallback callback) {
    trimmer_->TrimArcVmWorkingSet(std::move(callback),
                                  ArcVmReclaimType::kReclaimGuestPageCaches,
                                  arc::ArcSession::kNoPageLimit);
  }
  void TrimArcVmWorkingSetWithPageLimit(
      WorkingSetTrimmerChromeOS::TrimArcVmWorkingSetCallback callback,
      int page_limit) {
    trimmer_->TrimArcVmWorkingSet(std::move(callback),
                                  ArcVmReclaimType::kReclaimAll, page_limit);
  }

  void TearDownArcSessionManager() { arc_session_manager_.reset(); }

  arc::FakeMemoryInstance* memory_instance() { return &memory_instance_; }

  arc::ArcSessionRunner* arc_session_runner() {
    return arc_session_manager_->GetArcSessionRunnerForTesting();
  }

  static constexpr char kDefaultLocale[] = "en-US";
  arc::UpgradeParams DefaultUpgradeParams() {
    arc::UpgradeParams params;
    params.locale = kDefaultLocale;
    return params;
  }

  // Use this object within a code block that needs to interact with
  // the FakeSession within the ArcSessionRunner.
  // It is important to discard the session when done, even if errors
  // happen - so doing it in the destructor, to make it automatic.
  struct FakeArcSessionHolder {
    explicit FakeArcSessionHolder(arc::ArcSessionRunner* runner)
        : runner_(runner) {
      runner_->MakeArcSessionForTesting();
    }
    FakeArcSessionHolder(const FakeArcSessionHolder&) = delete;
    FakeArcSessionHolder& operator=(const FakeArcSessionHolder&) = delete;
    ~FakeArcSessionHolder() { runner_->DiscardArcSessionForTesting(); }
    arc::FakeArcSession* session() {
      return static_cast<arc::FakeArcSession*>(
          runner_->GetArcSessionForTesting());
    }
    arc::ArcSessionRunner* runner_;
  };

  std::unique_ptr<WorkingSetTrimmerChromeOS> trimmer_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  arc::ArcServiceManager arc_service_manager_;
  arc::FakeMemoryInstance memory_instance_;
  std::unique_ptr<arc::ArcSessionManager> arc_session_manager_;
  std::unique_ptr<TestingProfile> testing_profile_;
};

namespace {

// Tests that TrimArcVmWorkingSet runs the passed callback,
// and that the page limit is passed as requested.
TEST_F(TestWorkingSetTrimmerChromeOS, TrimArcVmWorkingSet) {
  absl::optional<bool> result;
  std::string reason;

  {
    FakeArcSessionHolder session_holder(arc_session_runner());
    session_holder.session()->set_trim_result(true, "test_reason");
    TrimArcVmWorkingSetWithPageLimit(
        base::BindLambdaForTesting(
            [&result, &reason](bool disposition, const std::string& status) {
              result = disposition;
              reason = status;
            }),
        5003);
    base::RunLoop().RunUntilIdle();
    ASSERT_TRUE(result);
    EXPECT_TRUE(*result);
    EXPECT_EQ(reason, "test_reason");
    EXPECT_EQ(session_holder.session()->trim_vm_memory_count(), 1);
    EXPECT_EQ(session_holder.session()->last_trim_vm_page_limit(), 5003);
  }
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

// Tests that TrimArcVmWorkingSetDropPageCachesOnly runs the passed callback.
TEST_F(TestWorkingSetTrimmerChromeOS, TrimArcVmWorkingSetDropPageCachesOnly) {
  absl::optional<bool> result;
  TrimArcVmWorkingSetDropPageCachesOnly(base::BindLambdaForTesting(
      [&result](bool r, const std::string&) { result = r; }));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(result);
  EXPECT_TRUE(*result);
}

// Tests that TrimArcVmWorkingSetDropPageCachesOnly runs the passed callback
// with false (failure) when DropCaches() fails.
TEST_F(TestWorkingSetTrimmerChromeOS,
       TrimArcVmWorkingSetDropPageCachesOnly_DropCachesFailure) {
  // Inject the failure.
  memory_instance()->set_drop_caches_result(false);

  absl::optional<bool> result;
  TrimArcVmWorkingSetDropPageCachesOnly(base::BindLambdaForTesting(
      [&result](bool r, const std::string&) { result = r; }));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(result);
  EXPECT_FALSE(*result);
}

// Tests that TrimArcVmWorkingSetDropPageCachesOnly runs the passed callback
// even when BrowserContext is not available.
TEST_F(TestWorkingSetTrimmerChromeOS,
       TrimArcVmWorkingSetDropPageCachesOnly_NoBrowserContext) {
  // Create a trimmer again with a null BrowserContext to make it unavailable.
  CreateTrimmer(nullptr);

  absl::optional<bool> result;
  TrimArcVmWorkingSetDropPageCachesOnly(base::BindLambdaForTesting(
      [&result](bool r, const std::string&) { result = r; }));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(result);
  // Expect false because dropping caches is not possible without a browser
  // context.
  EXPECT_FALSE(*result);
}

// Tests that TrimArcVmWorkingSetDropPageCachesOnly runs the passed callback
// even when ArcMemoryBridge is not available.
TEST_F(TestWorkingSetTrimmerChromeOS,
       TrimArcVmWorkingSetDropPageCachesOnly_NoArcMemoryBridge) {
  // Create a trimmer again with a different profile (BrowserContext) to make
  // ArcMemoryBridge unavailable.
  TestingProfile another_profile;
  CreateTrimmer(&another_profile);

  absl::optional<bool> result;
  TrimArcVmWorkingSetDropPageCachesOnly(base::BindLambdaForTesting(
      [&result](bool r, const std::string&) { result = r; }));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(result);
  // Expect false because dropping caches is not possible without a memory
  // bridge.
  EXPECT_FALSE(*result);

  trimmer_.reset();
}

// Tests that TrimArcVmWorkingSetDropPageCachesOnly runs the passed callback
// even when ArcSessionManager is not available.
TEST_F(TestWorkingSetTrimmerChromeOS,
       TrimArcVmWorkingSetDropPageCachesOnly_NoArcSessionManager) {
  // Make ArcSessionManager unavailable.
  TearDownArcSessionManager();

  absl::optional<bool> result;
  TrimArcVmWorkingSetDropPageCachesOnly(base::BindLambdaForTesting(
      [&result](bool r, const std::string&) { result = r; }));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(result);
  // Expect true because dropping caches can be done without ArcSessionManager.
  // The manager is necessary only for the actual VM trimming.
  EXPECT_TRUE(*result);
}

}  // namespace
}  // namespace mechanism
}  // namespace performance_manager
