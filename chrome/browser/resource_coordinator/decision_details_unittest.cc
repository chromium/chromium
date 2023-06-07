// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/decision_details.h"

#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/metrics/public/mojom/ukm_interface.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace resource_coordinator {

TEST(DecisionDetailsTest, DecisionDetailsReason) {
  // Ensure a null constructed reason work as expected.
  DecisionDetails::Reason reason;
  EXPECT_FALSE(reason.IsValid());
  EXPECT_FALSE(reason.IsSuccess());
  EXPECT_FALSE(reason.IsFailure());
  EXPECT_FALSE(reason.ToString());

  // Expect that copying works.
  reason = DecisionDetails::Reason(DecisionFailureReason::GLOBAL_DISALLOWLIST);
  EXPECT_TRUE(reason.IsValid());
  EXPECT_FALSE(reason.IsSuccess());
  EXPECT_TRUE(reason.IsFailure());
  EXPECT_EQ(ToString(DecisionFailureReason::GLOBAL_DISALLOWLIST),
            reason.ToString());
  EXPECT_EQ(DecisionFailureReason::GLOBAL_DISALLOWLIST,
            reason.failure_reason());

  // Ensure that the copy constructor works.
  DecisionDetails::Reason reason2(reason);
  EXPECT_TRUE(reason2.IsValid());
  EXPECT_FALSE(reason2.IsSuccess());
  EXPECT_TRUE(reason2.IsFailure());
  EXPECT_EQ(ToString(DecisionFailureReason::GLOBAL_DISALLOWLIST),
            reason2.ToString());
  EXPECT_EQ(DecisionFailureReason::GLOBAL_DISALLOWLIST,
            reason2.failure_reason());

  // Also check that (in)equality operators work.
  EXPECT_TRUE(reason == reason2);
  EXPECT_FALSE(reason != reason2);

  // Ensure failure reasons work as expected.
  for (size_t i = 0; i < static_cast<size_t>(DecisionFailureReason::MAX); ++i) {
    DecisionFailureReason failure_reason =
        static_cast<DecisionFailureReason>(i);
    reason = DecisionDetails::Reason(failure_reason);
    EXPECT_TRUE(reason.IsValid());
    EXPECT_FALSE(reason.IsSuccess());
    EXPECT_TRUE(reason.IsFailure());
    EXPECT_EQ(ToString(failure_reason), reason.ToString());
    EXPECT_EQ(failure_reason, reason.failure_reason());
  }

  // Ensure success reasons work as expected.
  for (size_t i = 0; i < static_cast<size_t>(DecisionSuccessReason::MAX); ++i) {
    DecisionSuccessReason success_reason =
        static_cast<DecisionSuccessReason>(i);
    reason = DecisionDetails::Reason(success_reason);
    EXPECT_TRUE(reason.IsValid());
    EXPECT_TRUE(reason.IsSuccess());
    EXPECT_FALSE(reason.IsFailure());
    EXPECT_EQ(ToString(success_reason), reason.ToString());
    EXPECT_EQ(success_reason, reason.success_reason());
  }
}

TEST(DecisionDetailsTest, DecisionDetails) {
  DecisionDetails details;
  std::vector<std::string> expected_failure_strings;
  expected_failure_strings.push_back(
      ToString(DecisionFailureReason::GLOBAL_DISALLOWLIST));

  // An empty decision is negative by default.
  EXPECT_EQ(0u, details.reasons().size());
  EXPECT_FALSE(details.IsPositive());

  // Adding a single failure reason makes it return negative.
  EXPECT_FALSE(details.AddReason(DecisionFailureReason::GLOBAL_DISALLOWLIST));
  EXPECT_EQ(1u, details.reasons().size());
  EXPECT_FALSE(details.IsPositive());
  EXPECT_EQ(DecisionFailureReason::GLOBAL_DISALLOWLIST,
            details.FailureReason());
  EXPECT_EQ(DecisionDetails::Reason(DecisionFailureReason::GLOBAL_DISALLOWLIST),
            details.reasons()[0]);
  EXPECT_EQ(expected_failure_strings, details.GetFailureReasonStrings());
  EXPECT_FALSE(details.toggled());

  // Adding a second failure reason doesn't change anything, but the failure
  // strings should grow.
  expected_failure_strings.push_back(
      ToString(DecisionFailureReason::ORIGIN_TRIAL_OPT_OUT));
  EXPECT_FALSE(details.AddReason(DecisionFailureReason::ORIGIN_TRIAL_OPT_OUT));
  EXPECT_EQ(2u, details.reasons().size());
  EXPECT_FALSE(details.IsPositive());
  EXPECT_EQ(DecisionFailureReason::GLOBAL_DISALLOWLIST,
            details.FailureReason());
  EXPECT_EQ(DecisionDetails::Reason(DecisionFailureReason::GLOBAL_DISALLOWLIST),
            details.reasons()[0]);
  EXPECT_EQ(
      DecisionDetails::Reason(DecisionFailureReason::ORIGIN_TRIAL_OPT_OUT),
      details.reasons()[1]);
  EXPECT_EQ(expected_failure_strings, details.GetFailureReasonStrings());
  EXPECT_FALSE(details.toggled());

  // Adding a success reason after this should have no effect, but the decision
  // chain should have toggled.
  EXPECT_TRUE(details.AddReason(DecisionSuccessReason::GLOBAL_ALLOWLIST));
  EXPECT_EQ(3u, details.reasons().size());
  EXPECT_FALSE(details.IsPositive());
  EXPECT_EQ(DecisionFailureReason::GLOBAL_DISALLOWLIST,
            details.FailureReason());
  EXPECT_EQ(DecisionDetails::Reason(DecisionFailureReason::GLOBAL_DISALLOWLIST),
            details.reasons()[0]);
  EXPECT_EQ(
      DecisionDetails::Reason(DecisionFailureReason::ORIGIN_TRIAL_OPT_OUT),
      details.reasons()[1]);
  EXPECT_EQ(DecisionDetails::Reason(DecisionSuccessReason::GLOBAL_ALLOWLIST),
            details.reasons()[2]);
  EXPECT_EQ(expected_failure_strings, details.GetFailureReasonStrings());
  EXPECT_TRUE(details.toggled());

  // Adding yet another failure after a success should not affect the failure
  // reason strings, as only failures before successes are emitted. Also, test
  // the AddReason member function that accessts a Reason directly rather than
  // an enum.
  EXPECT_TRUE(details.AddReason(
      DecisionDetails::Reason(DecisionFailureReason::HEURISTIC_AUDIO)));
  EXPECT_EQ(4u, details.reasons().size());
  EXPECT_FALSE(details.IsPositive());
  EXPECT_EQ(DecisionFailureReason::GLOBAL_DISALLOWLIST,
            details.FailureReason());
  EXPECT_EQ(DecisionDetails::Reason(DecisionFailureReason::GLOBAL_DISALLOWLIST),
            details.reasons()[0]);
  EXPECT_EQ(
      DecisionDetails::Reason(DecisionFailureReason::ORIGIN_TRIAL_OPT_OUT),
      details.reasons()[1]);
  EXPECT_EQ(DecisionDetails::Reason(DecisionSuccessReason::GLOBAL_ALLOWLIST),
            details.reasons()[2]);
  EXPECT_EQ(DecisionDetails::Reason(DecisionFailureReason::HEURISTIC_AUDIO),
            details.reasons()[3]);
  EXPECT_EQ(expected_failure_strings, details.GetFailureReasonStrings());
  EXPECT_TRUE(details.toggled());

  // Clear and go back to the initial state.
  details.Clear();
  EXPECT_EQ(0u, details.reasons().size());
  EXPECT_FALSE(details.IsPositive());
  EXPECT_FALSE(details.toggled());

  // Adding a single success reason makes it return positive. There should be no
  // failure reason strings.
  EXPECT_FALSE(details.AddReason(DecisionSuccessReason::GLOBAL_ALLOWLIST));
  EXPECT_EQ(1u, details.reasons().size());
  EXPECT_TRUE(details.IsPositive());
  EXPECT_EQ(DecisionSuccessReason::GLOBAL_ALLOWLIST, details.SuccessReason());
  EXPECT_EQ(DecisionDetails::Reason(DecisionSuccessReason::GLOBAL_ALLOWLIST),
            details.reasons()[0]);
  EXPECT_TRUE(details.GetFailureReasonStrings().empty());
  EXPECT_FALSE(details.toggled());

  // Adding a failure reason after this should have no effect, but the toggle
  // should be noted. There should still be no failure reason strings.
  EXPECT_TRUE(details.AddReason(DecisionFailureReason::GLOBAL_DISALLOWLIST));
  EXPECT_EQ(2u, details.reasons().size());
  EXPECT_TRUE(details.IsPositive());
  EXPECT_EQ(DecisionSuccessReason::GLOBAL_ALLOWLIST, details.SuccessReason());
  EXPECT_EQ(DecisionDetails::Reason(DecisionSuccessReason::GLOBAL_ALLOWLIST),
            details.reasons()[0]);
  EXPECT_EQ(DecisionDetails::Reason(DecisionFailureReason::GLOBAL_DISALLOWLIST),
            details.reasons()[1]);
  EXPECT_TRUE(details.GetFailureReasonStrings().empty());
  EXPECT_TRUE(details.toggled());
}

TEST(DecisionDetailsTest, TabManagerLifecycleStateChangeUkm) {
  DecisionDetails details;

  // Make details with every possible failure reason except one, followed by a
  // single success reason.
  EXPECT_FALSE(details.AddReason(
      DecisionFailureReason::LIFECYCLES_ENTERPRISE_POLICY_OPT_OUT));
  EXPECT_FALSE(details.AddReason(DecisionFailureReason::ORIGIN_TRIAL_OPT_OUT));
  EXPECT_FALSE(details.AddReason(DecisionFailureReason::GLOBAL_DISALLOWLIST));
  EXPECT_FALSE(details.AddReason(DecisionFailureReason::HEURISTIC_AUDIO));
  EXPECT_FALSE(details.AddReason(DecisionFailureReason::HEURISTIC_FAVICON));
  EXPECT_FALSE(details.AddReason(
      DecisionFailureReason::HEURISTIC_INSUFFICIENT_OBSERVATION));
  EXPECT_FALSE(details.AddReason(DecisionFailureReason::HEURISTIC_TITLE));
  EXPECT_FALSE(details.AddReason(DecisionFailureReason::LIVE_STATE_CAPTURING));
  // Skipping LIVE_STATE_EXTENSION_DISALLOWED here.
  EXPECT_FALSE(details.AddReason(DecisionFailureReason::LIVE_STATE_FORM_ENTRY));
  EXPECT_FALSE(details.AddReason(DecisionFailureReason::LIVE_STATE_IS_PDF));
  EXPECT_FALSE(details.AddReason(DecisionFailureReason::LIVE_STATE_MIRRORING));
  EXPECT_FALSE(
      details.AddReason(DecisionFailureReason::LIVE_STATE_PLAYING_AUDIO));
  EXPECT_FALSE(
      details.AddReason(DecisionFailureReason::LIVE_STATE_USING_WEB_SOCKETS));
  EXPECT_FALSE(
      details.AddReason(DecisionFailureReason::LIVE_STATE_USING_WEB_USB));
  EXPECT_FALSE(details.AddReason(DecisionFailureReason::LIVE_STATE_VISIBLE));
  EXPECT_FALSE(
      details.AddReason(DecisionFailureReason::LIVE_STATE_DEVTOOLS_OPEN));
  EXPECT_FALSE(
      details.AddReason(DecisionFailureReason::LIVE_STATE_DESKTOP_CAPTURE));
  EXPECT_FALSE(
      details.AddReason(DecisionFailureReason::LIVE_STATE_USING_BLUETOOTH));
  EXPECT_TRUE(details.AddReason(DecisionSuccessReason::ORIGIN_TRIAL_OPT_IN));

  // Dump the data to a UKM builder.
  ukm::TestUkmRecorder ukm_recorder;
  ukm::SourceId ukm_source_id = ukm::UkmRecorder::GetNewSourceID();
  ukm::builders::TabManager_LifecycleStateChange ukm_builder(ukm_source_id);
  details.Populate(&ukm_builder);
  ukm_builder.Record(&ukm_recorder);

  // Validate the output.
  EXPECT_EQ(1u, ukm_recorder.entries_count());
  auto entries = ukm_recorder.GetEntriesByName(ukm_builder.kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0].get();
  ukm_recorder.ExpectEntryMetric(
      entry, ukm_builder.kFailureLifecyclesEnterprisePolicyOptOutName, 1);
  ukm_recorder.ExpectEntryMetric(entry,
                                 ukm_builder.kFailureOriginTrialOptOutName, 1);
  ukm_recorder.ExpectEntryMetric(entry,
                                 ukm_builder.kFailureGlobalDisallowlistName, 1);
  ukm_recorder.ExpectEntryMetric(entry, ukm_builder.kFailureHeuristicAudioName,
                                 1);
  ukm_recorder.ExpectEntryMetric(entry,
                                 ukm_builder.kFailureHeuristicFaviconName, 1);
  ukm_recorder.ExpectEntryMetric(
      entry, ukm_builder.kFailureHeuristicInsufficientObservationName, 1);
  ukm_recorder.ExpectEntryMetric(entry, ukm_builder.kFailureHeuristicTitleName,
                                 1);
  ukm_recorder.ExpectEntryMetric(entry,
                                 ukm_builder.kFailureLiveStateCapturingName, 1);
  EXPECT_FALSE(ukm_recorder.EntryHasMetric(
      entry, ukm_builder.kFailureLiveStateExtensionDisallowedName));
  ukm_recorder.ExpectEntryMetric(entry,
                                 ukm_builder.kFailureLiveStateFormEntryName, 1);
  ukm_recorder.ExpectEntryMetric(entry, ukm_builder.kFailureLiveStateIsPDFName,
                                 1);
  ukm_recorder.ExpectEntryMetric(entry,
                                 ukm_builder.kFailureLiveStateMirroringName, 1);
  ukm_recorder.ExpectEntryMetric(
      entry, ukm_builder.kFailureLiveStatePlayingAudioName, 1);
  ukm_recorder.ExpectEntryMetric(
      entry, ukm_builder.kFailureLiveStateUsingWebSocketsName, 1);
  ukm_recorder.ExpectEntryMetric(
      entry, ukm_builder.kFailureLiveStateUsingWebUSBName, 1);
  ukm_recorder.ExpectEntryMetric(entry,
                                 ukm_builder.kFailureLiveStateVisibleName, 1);
  ukm_recorder.ExpectEntryMetric(
      entry, ukm_builder.kFailureLiveStateDevToolsOpenName, 1);
  ukm_recorder.ExpectEntryMetric(
      entry, ukm_builder.kFailureLiveStateDesktopCaptureName, 1);
  ukm_recorder.ExpectEntryMetric(
      entry, ukm_builder.kFailureLiveStateUsingBluetoothName, 1);
  EXPECT_FALSE(ukm_recorder.EntryHasMetric(
      entry, ukm_builder.kSuccessOriginTrialOptInName));
  EXPECT_FALSE(ukm_recorder.EntryHasMetric(
      entry, ukm_builder.kSuccessGlobalAllowlistName));
}

}  // namespace resource_coordinator
