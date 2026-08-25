// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_warming_checks.h"

#include <string>

#include "base/test/scoped_amount_of_physical_memory_override.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/glic/host/glic_web_contents_warming_pool.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/service/glic_instance_coordinator_impl.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "content/public/test/browser_test.h"
#include "net/base/network_change_notifier.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {
namespace {

// Base fixture providing common setup and helpers for warming checks tests.
class GlicWarmingChecksTestBase : public GlicBrowserTest {
 public:
  void SetUp() override {
    // Prevent premature preloading during startup.
    SetPrewarmingEnabledForTesting(false);
    ForceConnectionTypeForTesting(
        net::NetworkChangeNotifier::ConnectionType::CONNECTION_WIFI);
    GlicBrowserTest::SetUp();
  }

  void TearDown() override {
    GlicBrowserTest::TearDown();
    SetPrewarmingEnabledForTesting(true);
    ForceConnectionTypeForTesting(std::nullopt);
  }

  void ResetPrewarming() { SetPrewarmingEnabledForTesting(true); }

  GlicPrewarmingChecksResult RunShouldPreload() {
    base::test::TestFuture<GlicPrewarmingChecksResult> future;
    ShouldPreloadForProfile(GetProfile(), future.GetCallback());
    return future.Get();
  }

  void SetConnectionType(
      net::NetworkChangeNotifier::ConnectionType connection_type) {
    ForceConnectionTypeForTesting(connection_type);
  }

  GlicKeyedService* service() { return GlicKeyedService::Get(GetProfile()); }

  bool IsWarmed() {
    return coordinator()
        .GetWebContentsWarmingPoolForTesting()
        .HasWarmedContainerForTesting();
  }
};

// Fixture for verifying behavior when prewarming is disabled.
class GlicWarmingChecksDisabledBrowserTest : public GlicWarmingChecksTestBase {
 public:
  GlicWarmingChecksDisabledBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kGlicAnchorEntryPointForOnboardedUsers},
        /*disabled_features=*/{features::kGlicWarming});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Fixture for tests with prewarming enabled.
class GlicWarmingChecksBrowserTest : public GlicWarmingChecksTestBase {
 public:
  explicit GlicWarmingChecksBrowserTest(
      const std::string& delay_ms = "0",
      const std::string& min_required_ram_mb = "0") {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/{{features::kGlicWarming,
                               {{features::kGlicWarmingDelayMs.name, delay_ms},
                                {features::kGlicWarmingJitterMs.name, "0"},
                                {features::kGlicWarmingMinRequiredRamMb.name,
                                 min_required_ram_mb}}},
                              {features::kGlicAnchorEntryPointForOnboardedUsers,
                               {}}},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicWarmingChecksDisabledBrowserTest,
                       ShouldPreloadForProfile_WarmingDisabled) {
  EXPECT_EQ(RunShouldPreload(), GlicPrewarmingChecksResult::kWarmingDisabled);
}

IN_PROC_BROWSER_TEST_F(GlicWarmingChecksBrowserTest,
                       ShouldPreloadForProfile_Success) {
  ResetPrewarming();
  EXPECT_EQ(RunShouldPreload(), GlicPrewarmingChecksResult::kSuccess);
}

IN_PROC_BROWSER_TEST_F(GlicWarmingChecksBrowserTest,
                       ShouldPreloadForProfile_NotSupportedProfile) {
  ResetPrewarming();
  SetGlicCapability(GetProfile(), false);
  EXPECT_EQ(RunShouldPreload(),
            GlicPrewarmingChecksResult::kProfileNotEligibleAccountCapabilities);
}

IN_PROC_BROWSER_TEST_F(GlicWarmingChecksBrowserTest,
                       ShouldPreloadForProfile_WillBeDestroyed) {
  ResetPrewarming();
  GetProfile()->NotifyWillBeDestroyed();
  EXPECT_EQ(RunShouldPreload(),
            GlicPrewarmingChecksResult::kBrowserShuttingDown);
}

IN_PROC_BROWSER_TEST_F(GlicWarmingChecksBrowserTest,
                       ShouldPreloadForProfile_Cellular) {
  ResetPrewarming();
  SetConnectionType(net::NetworkChangeNotifier::ConnectionType::CONNECTION_2G);
  EXPECT_EQ(RunShouldPreload(),
            GlicPrewarmingChecksResult::kCellularConnection);
}

// Checks that we don't defer preloading when there's no delay.
IN_PROC_BROWSER_TEST_F(GlicWarmingChecksBrowserTest,
                       ShouldPreloadForProfile_DoNotDefer) {
  ResetPrewarming();
  service()->TryPreload();
  EXPECT_TRUE(
      RunUntil([this]() { return IsWarmed(); }, "Wait for container to warm"));
}

class GlicWarmingChecksLowMemoryTest : public GlicWarmingChecksBrowserTest {
 public:
  GlicWarmingChecksLowMemoryTest()
      : GlicWarmingChecksBrowserTest(/*delay_ms=*/"0",
                                     /*min_required_ram_mb=*/"4096") {}
  ~GlicWarmingChecksLowMemoryTest() override = default;
};

IN_PROC_BROWSER_TEST_F(GlicWarmingChecksLowMemoryTest,
                       ShouldPreloadForProfile_LowMemoryDevice) {
  ResetPrewarming();

  // Set the physical memory override to 2GB (2048MB), which is less than 4GB.
  base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
      base::GiB(2));

  EXPECT_EQ(RunShouldPreload(), GlicPrewarmingChecksResult::kDeviceLowMemory);
}

IN_PROC_BROWSER_TEST_F(GlicWarmingChecksLowMemoryTest,
                       ShouldPreloadForProfile_SufficientMemory) {
  ResetPrewarming();

  // Set the physical memory override to 8GB (8192MB), which is greater than
  // 4GB.
  base::test::ScopedAmountOfPhysicalMemoryOverride memory_override(
      base::GiB(8));

  EXPECT_EQ(RunShouldPreload(), GlicPrewarmingChecksResult::kSuccess);
}

class GlicWarmingChecksDeferredTest : public GlicWarmingChecksBrowserTest {
 public:
  // This sets the delay to 500 ms.
  GlicWarmingChecksDeferredTest()
      : GlicWarmingChecksBrowserTest(/*delay_ms=*/"500") {}
  ~GlicWarmingChecksDeferredTest() override = default;
};

// Checks that if we have a preload delay, that we won't preload immediately.
IN_PROC_BROWSER_TEST_F(GlicWarmingChecksDeferredTest,
                       ShouldPreloadForProfile_Defer) {
  ResetPrewarming();
  service()->TryPreload();
  // Wait for an interval shorter than the configured 500ms preload delay to
  // verify that warming has not triggered prematurely while deferred.
  WaitForDuration(base::Milliseconds(200));
  EXPECT_FALSE(IsWarmed());
}

IN_PROC_BROWSER_TEST_F(GlicWarmingChecksDeferredTest,
                       ShouldPreloadForProfile_DeferWithProfileDeletion) {
  ResetPrewarming();
  base::test::TestFuture<void> future;
  service()->AddPreloadCallback(future.GetCallback());
  service()->TryPreload();
  service()->reset_profile_for_test();
  EXPECT_TRUE(future.Wait());
  EXPECT_FALSE(IsWarmed());
}

}  // namespace
}  // namespace glic
