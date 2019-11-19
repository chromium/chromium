// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/antivirus_metrics_provider_win.h"

#include <vector>

#include "base/bind.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_restrictions.h"
#include "base/version.h"
#include "base/win/windows_version.h"
#include "chrome/services/util_win/util_win_impl.h"
#include "components/variations/hashing.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

void VerifySystemProfileData(const metrics::SystemProfileProto& system_profile,
                             bool expect_unhashed_value) {
  if (base::win::GetVersion() < base::win::Version::WIN8)
    return;

  // The name of Windows Defender changed sometime in Windows 10, so any of the
  // following is possible.
  constexpr char kWindowsDefender[] = "Windows Defender";
  constexpr char kWindowsDefenderAntivirus[] = "Windows Defender Antivirus";

  if (base::win::GetVersion() >= base::win::Version::WIN8) {
    bool defender_found = false;
    for (const auto& av : system_profile.antivirus_product()) {
      if (av.product_name_hash() ==
          variations::HashName(kWindowsDefender) ||
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
    EXPECT_TRUE(defender_found);
  }
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

  void GetMetricsCallback() {
    // Check that the callback runs on the main loop.
    ASSERT_TRUE(thread_checker_.CalledOnValidThread());

    got_results_ = true;

    metrics::SystemProfileProto system_profile;
    provider_.ProvideSystemProfileMetrics(&system_profile);

    VerifySystemProfileData(system_profile, expect_unhashed_value_);
    // This looks weird, but it's to make sure that reading the data out of the
    // AntiVirusMetricsProvider does not invalidate it, as the class should be
    // resilient to this.
    system_profile.Clear();
    provider_.ProvideSystemProfileMetrics(&system_profile);
    VerifySystemProfileData(system_profile, expect_unhashed_value_);
  }

  // Helper function to toggle whether the ReportFullAVProductDetails feature is
  // enabled or not.
  void SetFullNamesFeatureEnabled(bool enabled) {
    if (enabled) {
      scoped_feature_list_.InitAndEnableFeature(
          AntiVirusMetricsProvider::kReportNamesFeature);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          AntiVirusMetricsProvider::kReportNamesFeature);
    }
  }

  bool got_results_;
  bool expect_unhashed_value_;
  base::test::TaskEnvironment task_environment_;
  base::Optional<UtilWinImpl> util_win_impl_;
  AntiVirusMetricsProvider provider_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::ThreadCheckerImpl thread_checker_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AntiVirusMetricsProviderTest);
};

TEST_P(AntiVirusMetricsProviderTest, GetMetricsFullName) {
  base::ScopedAllowBlockingForTesting scoped_allow_blocking_;

  ASSERT_TRUE(thread_checker_.CalledOnValidThread());
  SetFullNamesFeatureEnabled(expect_unhashed_value_);

  // The usage of base::Unretained(this) is safe here because |provider_|, who
  // owns the callback, will go away before |this|.
  provider_.AsyncInit(
      base::Bind(&AntiVirusMetricsProviderTest::GetMetricsCallback,
                 base::Unretained(this)));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(got_results_);
}
