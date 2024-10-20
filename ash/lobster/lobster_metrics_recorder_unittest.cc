// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/lobster/lobster_metrics_recorder.h"

#include "ash/public/cpp/lobster/lobster_metrics_state_enums.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

class LobsterMetricsRecorderTest : public testing::Test {
 public:
  LobsterMetricsRecorderTest() = default;
  ~LobsterMetricsRecorderTest() override = default;
  base::HistogramTester histogram_tester_;

 private:
  base::test::TaskEnvironment task_environment_;
};

class LobsterStateMetrics
    : public LobsterMetricsRecorderTest,
      public testing::WithParamInterface<LobsterMetricState> {};

INSTANTIATE_TEST_SUITE_P(
    LobsterMetricsRecorderTest,
    LobsterStateMetrics,
    testing::ValuesIn<LobsterMetricState>({
        LobsterMetricState::kShownOpportunity,
        LobsterMetricState::kBlocked,
        LobsterMetricState::kRightClickTriggerImpression,
        LobsterMetricState::kRightClickTriggerFired,
        LobsterMetricState::kRightClickTriggerNeedsConsent,
        LobsterMetricState::kPickerTriggerImpression,
        LobsterMetricState::kPickerTriggerFired,
        LobsterMetricState::kPickerTriggerNeedsConsent,
        LobsterMetricState::kConsentScreenImpression,
        LobsterMetricState::kConsentAccepted,
        LobsterMetricState::kConsentRejected,
        LobsterMetricState::kQueryPageImpression,
        LobsterMetricState::kRequestInitialCandidates,
        LobsterMetricState::kRequestInitialCandidatesSuccess,
        LobsterMetricState::kRequestInitialCandidatesError,
        LobsterMetricState::kInitialCandidatesImpression,
        LobsterMetricState::kRequestMoreCandidates,
        LobsterMetricState::kRequestMoreCandidatesSuccess,
        LobsterMetricState::kRequestMoreCandidatesError,
        LobsterMetricState::kMoreCandidatesAppended,
        LobsterMetricState::kCandidateDownload,
        LobsterMetricState::kCandidateDownloadSuccess,
        LobsterMetricState::kCandidateDownloadError,
        LobsterMetricState::kCommitAsDownloadSuccess,
        LobsterMetricState::kCommitAsDownloadError,
        LobsterMetricState::kCommitAsInsert,
        LobsterMetricState::kCommitAsInsertSuccess,
        LobsterMetricState::kCommitAsInsertError,
    }));

TEST_P(LobsterStateMetrics, RecordWhenInvoked) {
  LobsterMetricState state = GetParam();
  ash::RecordLobsterState(state);

  histogram_tester_.ExpectBucketCount("Ash.Lobster.State", state, 1);
}

}  // namespace

}  // namespace ash
