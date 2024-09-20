// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/mechanisms/working_set_trimmer_chromeos.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/memory/arc_memory_bridge.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "ash/components/arc/test/fake_arc_session.h"
#include "ash/components/arc/test/fake_memory_instance.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/arc/test/test_arc_session_manager.h"
#include "chrome/browser/ash/arc/vmm/arcvm_working_set_trim_executor.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {
namespace mechanism {

class TestWorkingSetTrimmerChromeOS : public testing::Test {
 public:
  TestWorkingSetTrimmerChromeOS()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
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
    raw_ptr<arc::ArcSessionRunner> runner_;
  };

  content::BrowserTaskEnvironment& task_environment() {
    return task_environment_;
  }

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
  std::optional<bool> result;
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

  std::optional<bool> result;
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

  std::optional<bool> result;
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

  std::optional<bool> result;
  TrimArcVmWorkingSet(base::BindLambdaForTesting(
      [&result](bool r, const std::string&) { result = r; }));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(result);
  EXPECT_FALSE(*result);
}

TEST_F(TestWorkingSetTrimmerChromeOS,
       TrimArcVmWorkingSet_GuestReclaimEnabled_Success) {
  base::HistogramTester histogram_tester;
  base::ScopedMockElapsedTimersForTest mock_elapsed_timers;
  base::test::ScopedFeatureList feature_list;
  base::FieldTrialParams params;
  params["guest_reclaim_enabled"] = "true";
  feature_list.InitAndEnableFeatureWithParameters(arc::kGuestSwap, params);
  memory_instance()->set_reclaim_all_result(2, 1);
  // Making arc session trim result to be false to be sure it's not being used.
  FakeArcSessionHolder session_holder(arc_session_runner());
  session_holder.session()->set_trim_result(false, "test_reason");

  std::optional<bool> result;
  TrimArcVmWorkingSet(base::BindLambdaForTesting(
      [&result](bool r, const std::string&) { result = r; }));
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectUniqueSample("Arc.GuestZram.SuccessfulReclaim", 1, 1);
  histogram_tester.ExpectUniqueSample("Arc.GuestZram.ReclaimedProcess", 2, 1);
  histogram_tester.ExpectUniqueSample("Arc.GuestZram.UnreclaimedProcess", 1, 1);
  histogram_tester.ExpectUniqueTimeSample(
      "Arc.GuestZram.TotalReclaimTime",
      base::ScopedMockElapsedTimersForTest::kMockElapsedTime, 1);
  ASSERT_TRUE(result);
  ASSERT_TRUE(*result);
}

TEST_F(TestWorkingSetTrimmerChromeOS,
       TrimArcVmWorkingSet_GuestReclaimEnabled_Failure) {
  base::HistogramTester histogram_tester;
  base::ScopedMockElapsedTimersForTest mock_elapsed_timers;
  base::test::ScopedFeatureList feature_list;
  base::FieldTrialParams params;
  params["guest_reclaim_enabled"] = "true";
  feature_list.InitAndEnableFeatureWithParameters(arc::kGuestSwap, params);
  memory_instance()->set_reclaim_all_result(0, 0);

  std::optional<bool> result;
  TrimArcVmWorkingSet(base::BindLambdaForTesting(
      [&result](bool r, const std::string&) { result = r; }));
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectUniqueSample("Arc.GuestZram.SuccessfulReclaim", 0, 1);
  histogram_tester.ExpectUniqueSample("Arc.GuestZram.ReclaimedProcess", 0, 0);
  histogram_tester.ExpectUniqueSample("Arc.GuestZram.UnreclaimedProcess", 0, 0);
  histogram_tester.ExpectUniqueTimeSample(
      "Arc.GuestZram.TotalReclaimTime",
      base::ScopedMockElapsedTimersForTest::kMockElapsedTime, 0);
  ASSERT_TRUE(result);
  ASSERT_FALSE(*result);
}

TEST_F(TestWorkingSetTrimmerChromeOS,
       TrimArcVmWorkingSet_GuestReclaimEnabled_AnonPagesOnly) {
  base::HistogramTester histogram_tester;
  base::ScopedMockElapsedTimersForTest mock_elapsed_timers;
  base::test::ScopedFeatureList feature_list;
  base::FieldTrialParams params;
  params["guest_reclaim_enabled"] = "true";
  params["guest_reclaim_only_anonymous"] = "true";
  feature_list.InitAndEnableFeatureWithParameters(arc::kGuestSwap, params);
  memory_instance()->set_reclaim_all_result(0, 0);
  memory_instance()->set_reclaim_anon_result(2, 0);

  std::optional<bool> result;
  TrimArcVmWorkingSet(base::BindLambdaForTesting(
      [&result](bool r, const std::string&) { result = r; }));
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectUniqueSample("Arc.GuestZram.SuccessfulReclaim", 1, 1);
  histogram_tester.ExpectUniqueSample("Arc.GuestZram.ReclaimedProcess", 2, 1);
  histogram_tester.ExpectUniqueSample("Arc.GuestZram.UnreclaimedProcess", 0, 1);
  histogram_tester.ExpectUniqueTimeSample(
      "Arc.GuestZram.TotalReclaimTime",
      base::ScopedMockElapsedTimersForTest::kMockElapsedTime, 1);
  ASSERT_TRUE(result);
  ASSERT_TRUE(*result);
}

TEST_F(TestWorkingSetTrimmerChromeOS,
       TrimArcVmWorkingSet_GuestVirtualSwap_GuestReclaimSucceeded) {
  base::test::ScopedFeatureList feature_list;
  base::FieldTrialParams params;
  params["guest_reclaim_enabled"] = "true";
  params["virtual_swap_enabled"] = "true";
  feature_list.InitWithFeaturesAndParameters({{arc::kGuestSwap, params}},
                                             {arc::kLockGuestMemory});
  FakeArcSessionHolder session_holder(arc_session_runner());
  session_holder.session()->set_trim_result(true, "");
  // Guest reclaimed succeeded.
  memory_instance()->set_reclaim_all_result(2, 1);

  std::optional<bool> result;
  TrimArcVmWorkingSet(base::BindLambdaForTesting(
      [&result](bool r, const std::string&) { result = r; }));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(result);
  ASSERT_TRUE(*result);
  // Host reclaim should be invoked with virtual swap
  ASSERT_EQ(session_holder.session()->trim_vm_memory_count(), 1);
}

TEST_F(TestWorkingSetTrimmerChromeOS,
       TrimArcVmWorkingSet_GuestVirtualSwap_GuestReclaimFailed) {
  base::test::ScopedFeatureList feature_list;
  base::FieldTrialParams params;
  params["guest_reclaim_enabled"] = "true";
  params["virtual_swap_enabled"] = "true";
  feature_list.InitWithFeaturesAndParameters({{arc::kGuestSwap, params}},
                                             {arc::kLockGuestMemory});
  FakeArcSessionHolder session_holder(arc_session_runner());
  session_holder.session()->set_trim_result(true, "");
  // Guest reclaimed failed.
  memory_instance()->set_reclaim_all_result(0, 2);

  std::optional<bool> result;
  TrimArcVmWorkingSet(base::BindLambdaForTesting(
      [&result](bool r, const std::string&) { result = r; }));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(result);
  ASSERT_TRUE(*result);
  // Host reclaim should be invoked with virtual swap
  ASSERT_EQ(session_holder.session()->trim_vm_memory_count(), 1);
}

TEST_F(TestWorkingSetTrimmerChromeOS,
       TrimArcVmWorkingSet_GuestVirtualSwap_GuestMemoryLocked) {
  base::test::ScopedFeatureList feature_list;
  base::FieldTrialParams params;
  params["guest_reclaim_enabled"] = "true";
  params["virtual_swap_enabled"] = "true";
  feature_list.InitWithFeaturesAndParameters(
      {{arc::kGuestSwap, params}, {arc::kLockGuestMemory, {}}}, {});
  FakeArcSessionHolder session_holder(arc_session_runner());
  // Guest reclaimed succeeded.
  memory_instance()->set_reclaim_all_result(2, 0);

  std::optional<bool> result;
  TrimArcVmWorkingSet(base::BindLambdaForTesting(
      [&result](bool r, const std::string&) { result = r; }));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(result);
  ASSERT_TRUE(*result);
  // Host reclaim should not happen when guest memory is locked
  ASSERT_EQ(session_holder.session()->trim_vm_memory_count(), 0);
}

TEST_F(TestWorkingSetTrimmerChromeOS,
       TrimArcVmWorkingSet_GuestZramDisabled_ArcSessionIsUsed) {
  FakeArcSessionHolder session_holder(arc_session_runner());
  session_holder.session()->set_trim_result(true, "");
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(arc::kGuestSwap);
  // If memory_instance is used then the trim operation should fail.
  memory_instance()->set_reclaim_all_result(0, 0);

  std::optional<bool> result;
  TrimArcVmWorkingSet(base::BindLambdaForTesting(
      [&result](bool r, const std::string&) { result = r; }));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(result);
  ASSERT_TRUE(*result);
}

TEST_F(
    TestWorkingSetTrimmerChromeOS,
    TrimArcVmWorkingSet_GuestZramEnabledWithNoGuestReclaim_ArcSessionIsUsed) {
  FakeArcSessionHolder session_holder(arc_session_runner());
  session_holder.session()->set_trim_result(true, "");
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(arc::kGuestSwap);
  // If memory_instance is used then the trim operation should fail.
  memory_instance()->set_reclaim_all_result(0, 0);

  std::optional<bool> result;
  TrimArcVmWorkingSet(base::BindLambdaForTesting(
      [&result](bool r, const std::string&) { result = r; }));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(result);
  ASSERT_TRUE(*result);
}

// Tests that TrimArcVmWorkingSetDropPageCachesOnly runs the passed callback.
TEST_F(TestWorkingSetTrimmerChromeOS, TrimArcVmWorkingSetDropPageCachesOnly) {
  std::optional<bool> result;
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
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(arc::kSkipDropCaches);

  // Inject the failure.
  memory_instance()->set_drop_caches_result(false);

  std::optional<bool> result;
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

  std::optional<bool> result;
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

  std::optional<bool> result;
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

  std::optional<bool> result;
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
