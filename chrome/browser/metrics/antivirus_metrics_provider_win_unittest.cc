// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/antivirus_metrics_provider_win.h"

#include <optional>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_restrictions.h"
#include "base/version.h"
#include "base/win/scoped_com_initializer.h"
#include "chrome/services/util_win/util_win_impl.h"
#include "components/variations/hashing.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

void VerifySystemProfileData(const metrics::SystemProfileProto& system_profile,
                             bool expect_unhashed_value,
                             bool second_run) {
  // The name of Windows Defender changed sometime in Windows 10, so any of the
  // following is possible.
  constexpr char kWindowsDefender[] = "Windows Defender";
  constexpr char kWindowsDefenderAntivirus[] = "Windows Defender Antivirus";
  constexpr char kMicrosoftDefenderAntivirus[] = "Microsoft Defender Antivirus";

  bool defender_found = false;
  uint32_t last_hash = 0xdeadbeef;
  for (const auto& av : system_profile.antivirus_product()) {
    if (av.has_product_name_hash()) {
      last_hash = av.product_name_hash();
    }
    if (av.product_name_hash() == variations::HashName(kWindowsDefender) ||
        av.product_name_hash() ==
            variations::HashName(kWindowsDefenderAntivirus) ||
        av.product_name_hash() ==
            variations::HashName(kMicrosoftDefenderAntivirus)) {
      defender_found = true;
      if (expect_unhashed_value) {
        EXPECT_TRUE(av.has_product_name());
        EXPECT_TRUE(av.product_name() == kWindowsDefender ||
                    av.product_name() == kWindowsDefenderAntivirus ||
                    av.product_name() == kMicrosoftDefenderAntivirus);
      } else {
        EXPECT_FALSE(av.has_product_name());
      }
      break;
    }
  }
  EXPECT_TRUE(defender_found)
      << "expect_unhashed_value = " << expect_unhashed_value
      << ", second_run = " << second_run << ", "
      << system_profile.antivirus_product().size()
      << " antivirus products found. Last hash is " << last_hash << ".";
}

}  // namespace

class AntiVirusMetricsProviderTest : public ::testing::TestWithParam<bool> {
 public:
  AntiVirusMetricsProviderTest()
      : got_results_(false),
        expect_unhashed_value_(GetParam()),
        has_products_(false) {
    mojo::PendingRemote<chrome::mojom::UtilWin> remote;
    util_win_impl_.emplace(remote.InitWithNewPipeAndPassReceiver());
    provider_.SetRemoteUtilWinForTesting(std::move(remote));
  }

  AntiVirusMetricsProviderTest(const AntiVirusMetricsProviderTest&) = delete;
  AntiVirusMetricsProviderTest& operator=(const AntiVirusMetricsProviderTest&) =
      delete;

  void GetMetricsCallback() {
    // Check that the callback runs on the main loop.
    ASSERT_TRUE(thread_checker_.CalledOnValidThread());

    got_results_ = true;

    metrics::SystemProfileProto system_profile;
    provider_.ProvideSystemProfileMetrics(&system_profile);

    // If no antivirus products are found (for example in a CI VM or Windows
    // Server environment where Windows Security Center or Defender is
    // unavailable), gracefully skip the verification.
    has_products_ = !system_profile.antivirus_product().empty();
    if (!has_products_) {
      return;
    }

    VerifySystemProfileData(system_profile, expect_unhashed_value_, false);
    // This looks weird, but it's to make sure that reading the data out of the
    // AntiVirusMetricsProvider does not invalidate it, as the class should be
    // resilient to this.
    system_profile.Clear();
    provider_.ProvideSystemProfileMetrics(&system_profile);
    VerifySystemProfileData(system_profile, expect_unhashed_value_, true);
  }

  // Helper function to toggle whether the ReportFullAVProductDetails feature is
  // enabled or not.
  void SetFullNamesFeatureEnabled(bool enabled) {
    if (enabled) {
      scoped_feature_list_.InitAndEnableFeature(kReportFullAVProductDetails);
    } else {
      scoped_feature_list_.InitAndDisableFeature(kReportFullAVProductDetails);
    }
  }

  bool got_results_;
  bool expect_unhashed_value_;
  bool has_products_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
  std::optional<UtilWinImpl> util_win_impl_;
  AntiVirusMetricsProvider provider_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::ThreadCheckerImpl thread_checker_;
  base::HistogramTester histogram_tester_;
};

TEST_P(AntiVirusMetricsProviderTest, GetMetricsFullName) {
  base::ScopedAllowBlockingForTesting scoped_allow_blocking_;
  base::win::ScopedCOMInitializer com_initializer;

  ASSERT_TRUE(com_initializer.Succeeded());
  ASSERT_TRUE(thread_checker_.CalledOnValidThread());
  SetFullNamesFeatureEnabled(expect_unhashed_value_);

  // The usage of base::Unretained(this) is safe here because |provider_|, who
  // owns the callback, will go away before |this|.
  provider_.AsyncInit(
      base::BindOnce(&AntiVirusMetricsProviderTest::GetMetricsCallback,
                     base::Unretained(this)));
  task_environment_.RunUntilIdle();
  if (!has_products_) {
    GTEST_SKIP()
        << "No antivirus products found (Windows Security Center or "
           "Windows Defender is not available in this test environment).";
  }

  EXPECT_TRUE(got_results_);
  histogram_tester_.ExpectTotalCount("UMA.AntiVirusMetricsProvider.Latency", 1);
}

TEST_P(AntiVirusMetricsProviderTest, CallProvideMetricsBeforeAsyncInit) {
  AntiVirusMetricsProvider provider;
  metrics::SystemProfileProto system_profile;

  // Returns an empty list.
  provider.ProvideSystemProfileMetrics(&system_profile);
  EXPECT_EQ(system_profile.antivirus_product_size(), 0);
}

TEST_P(AntiVirusMetricsProviderTest, CallAsyncInitAfterCacheIsPopulated) {
  base::ScopedAllowBlockingForTesting scoped_allow_blocking;
  base::win::ScopedCOMInitializer com_initializer;

  ASSERT_TRUE(com_initializer.Succeeded());
  ASSERT_TRUE(thread_checker_.CalledOnValidThread());
  SetFullNamesFeatureEnabled(expect_unhashed_value_);

  // Call first query to populate the cache.
  provider_.AsyncInit(
      base::BindOnce(&AntiVirusMetricsProviderTest::GetMetricsCallback,
                     base::Unretained(this)));
  task_environment_.RunUntilIdle();
  if (!has_products_) {
    GTEST_SKIP()
        << "No antivirus products found (Windows Security Center or "
           "Windows Defender is not available in this test environment).";
  }

  EXPECT_TRUE(got_results_);
  histogram_tester_.ExpectTotalCount("UMA.AntiVirusMetricsProvider.Latency", 1);

  // Call second query to return cached results.
  bool callback_2 = false;
  AntiVirusMetricsProvider provider_2;
  provider_2.AsyncInit(
      base::BindOnce([](bool* ran) { *ran = true; }, &callback_2));
  EXPECT_TRUE(callback_2);

  // Verifies that data is available to the second instance.
  metrics::SystemProfileProto system_profile;
  provider_2.ProvideSystemProfileMetrics(&system_profile);
  EXPECT_GT(system_profile.antivirus_product_size(), 0);

  // Verify that histogram count did not change.
  histogram_tester_.ExpectTotalCount("UMA.AntiVirusMetricsProvider.Latency", 1);
}

// TODO(crbug.com/553292299): Re-enable when no longer flaky on Windows.
TEST_P(AntiVirusMetricsProviderTest, DISABLED_CallAsyncInitConcurrently) {
  base::ScopedAllowBlockingForTesting scoped_allow_blocking;
  base::win::ScopedCOMInitializer com_initializer;

  ASSERT_TRUE(com_initializer.Succeeded());
  ASSERT_TRUE(thread_checker_.CalledOnValidThread());
  SetFullNamesFeatureEnabled(expect_unhashed_value_);

  bool callback_1 = false;
  bool callback_2 = false;

  AntiVirusMetricsProvider provider_1;
  AntiVirusMetricsProvider provider_2;

  // Start the first query.
  provider_1.AsyncInit(
      base::BindOnce([](bool* ran) { *ran = true; }, &callback_1));

  // Start the second query while the first query is in-flight.
  provider_2.AsyncInit(
      base::BindOnce([](bool* ran) { *ran = true; }, &callback_2));

  EXPECT_FALSE(callback_1);
  EXPECT_FALSE(callback_2);

  // Wait for the single Mojo query to finish.
  task_environment_.RunUntilIdle();

  // Both callers are updated.
  EXPECT_TRUE(callback_1);
  EXPECT_TRUE(callback_2);

  // Verify that one histogram is recorded.
  histogram_tester_.ExpectTotalCount("UMA.AntiVirusMetricsProvider.Latency", 1);
}

INSTANTIATE_TEST_SUITE_P(, AntiVirusMetricsProviderTest, ::testing::Bool());
