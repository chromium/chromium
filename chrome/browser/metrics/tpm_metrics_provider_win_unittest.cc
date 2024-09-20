// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/tpm_metrics_provider_win.h"

#include <optional>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_restrictions.h"
#include "base/version.h"
#include "chrome/services/util_win/util_win_impl.h"
#include "components/variations/hashing.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

void VerifyNonEmptySystemProfileTPMData(
    const metrics::SystemProfileProto& system_profile) {
  EXPECT_FALSE(system_profile.tpm_identifier().tpm_specific_version().empty());
  EXPECT_FALSE(system_profile.tpm_identifier().manufacturer_version().empty());
  EXPECT_FALSE(
      system_profile.tpm_identifier().manufacturer_version_info().empty());
  EXPECT_NE(system_profile.tpm_identifier().manufacturer_id(), 0u);
}

}  // namespace

class TPMMetricsProviderTest : public testing::Test {
 public:
  TPMMetricsProviderTest() {
    scoped_feature_list_.InitAndEnableFeature(kReportFullTPMIdentifierDetails);
    mojo::PendingRemote<chrome::mojom::UtilWin> remote;
    util_win_impl_.emplace(remote.InitWithNewPipeAndPassReceiver());
    provider_.SetRemoteUtilWinForTesting(std::move(remote));
  }

  TPMMetricsProviderTest(const TPMMetricsProviderTest&) = delete;
  TPMMetricsProviderTest& operator=(const TPMMetricsProviderTest&) = delete;

  ~TPMMetricsProviderTest() override = default;

  void GetMetricsCallback() {
    // Check that the callback runs on the main loop.
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    got_results_ = true;

    metrics::SystemProfileProto system_profile;
    provider_.ProvideSystemProfileMetrics(&system_profile);

    VerifyNonEmptySystemProfileTPMData(system_profile);
  }

  bool got_results_;
  SEQUENCE_CHECKER(sequence_checker_);
  base::test::TaskEnvironment task_environment_;
  std::optional<UtilWinImpl> util_win_impl_;
  TPMMetricsProvider provider_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(TPMMetricsProviderTest, GetMetricsFullName) {
  base::ScopedAllowBlockingForTesting scoped_allow_blocking_;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  provider_.AsyncInit(base::BindOnce(
      &TPMMetricsProviderTest::GetMetricsCallback, base::Unretained(this)));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(got_results_);
}
