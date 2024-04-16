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
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_restrictions.h"
#include "base/version.h"
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

  bool defender_found = false;
  uint32_t last_hash = 0xdeadbeef;
  for (const auto& av : system_profile.antivirus_product()) {
    if (av.has_product_name_hash()) {
      last_hash = av.product_name_hash();
    }
    if (av.product_name_hash() == variations::HashName(kWindowsDefender) ||
        av.product_name_hash() ==
            variations::HashName(kWindowsDefenderAntivirus)) {
      defender_found = true;
      if (expect_unhashed_value) {
        EXPECT_TRUE(av.has_product_name());
        EXPECT_TRUE(av.product_name() == kWindowsDefender ||
                    av.product_name() == kWindowsDefenderAntivirus);
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
      : got_results_(false), expect_unhashed_value_(GetParam()) {
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
  base::test::TaskEnvironment task_environment_;
  std::optional<UtilWinImpl> util_win_impl_;
  AntiVirusMetricsProvider provider_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::ThreadCheckerImpl thread_checker_;
};

// TODO(crbug.com/41295648): Flaky on Windows 10.
TEST_P(AntiVirusMetricsProviderTest, DISABLED_GetMetricsFullName) {
  base::ScopedAllowBlockingForTesting scoped_allow_blocking_;

  ASSERT_TRUE(thread_checker_.CalledOnValidThread());
  SetFullNamesFeatureEnabled(expect_unhashed_value_);

  // The usage of base::Unretained(this) is safe here because |provider_|, who
  // owns the callback, will go away before |this|.
  provider_.AsyncInit(
      base::BindOnce(&AntiVirusMetricsProviderTest::GetMetricsCallback,
                     base::Unretained(this)));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(got_results_);
}

INSTANTIATE_TEST_SUITE_P(, AntiVirusMetricsProviderTest, ::testing::Bool());
