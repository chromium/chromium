// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_restore_metrics.h"

#include <optional>
#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "components/sessions/core/command_storage_read_status.h"
#include "components/sessions/core/session_service_commands.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using sessions::CommandStorageReadStatus;
using sessions::SessionReplayResult;

struct MappingTestCase {
  const char* name;
  CommandStorageReadStatus status;
  SessionReplayResult replay_result;
  bool any_windows_left;
  SessionRestoreOutcome expected_outcome;
  std::optional<SessionRestoreErrorReason> expected_reason;
};

// The rows of the mapping table. Together these cover every combination the
// storage and replay layers can actually produce.
const MappingTestCase kMappingTestCases[] = {
    {"NoFile", CommandStorageReadStatus::kNoFile,
     SessionReplayResult::kNoCommands, false,
     SessionRestoreOutcome::kNoSessionFile, std::nullopt},
    {"ReadError", CommandStorageReadStatus::kFileInvalid,
     SessionReplayResult::kNoCommands, false, SessionRestoreOutcome::kFailed,
     SessionRestoreErrorReason::kCorruptedOrIncompleteFile},
    {"EmptySession", CommandStorageReadStatus::kSuccess,
     SessionReplayResult::kNoCommands, false,
     SessionRestoreOutcome::kNothingToRestore, std::nullopt},
    {"AllWindowsClosedBeforeExit", CommandStorageReadStatus::kSuccess,
     SessionReplayResult::kAllCommandsClosed, false,
     SessionRestoreOutcome::kNothingToRestore, std::nullopt},
    {"CorruptedCommandWithoutWindows", CommandStorageReadStatus::kSuccess,
     SessionReplayResult::kCorruptedCommand, false,
     SessionRestoreOutcome::kFailed,
     SessionRestoreErrorReason::kCorruptedOrIncompleteCommand},
    // The case the metric exists for: windows came back, but the replay was
    // truncated, so tabs were silently lost.
    {"CorruptedCommandWithWindows", CommandStorageReadStatus::kSuccess,
     SessionReplayResult::kCorruptedCommand, true,
     SessionRestoreOutcome::kFailed,
     SessionRestoreErrorReason::kCorruptedOrIncompleteCommand},
    {"AllTabsPruned", CommandStorageReadStatus::kSuccess,
     SessionReplayResult::kAllTabsPrunedNoNavigations, false,
     SessionRestoreOutcome::kFailed,
     SessionRestoreErrorReason::kAllTabsPrunedNoNavigations},
    {"AllWindowsHadNoValidTabs", CommandStorageReadStatus::kSuccess,
     SessionReplayResult::kAllWindowsHadNoValidTabs, false,
     SessionRestoreOutcome::kFailed,
     SessionRestoreErrorReason::kAllWindowsHadNoValidTabs},
    {"Success", CommandStorageReadStatus::kSuccess,
     SessionReplayResult::kSuccess, true, SessionRestoreOutcome::kSuccess,
     std::nullopt},
    {"NoWindowsOfExpectedType", CommandStorageReadStatus::kSuccess,
     SessionReplayResult::kSuccess, false, SessionRestoreOutcome::kFailed,
     SessionRestoreErrorReason::kNoWindowsOfExpectedType},
};

class SessionRestoreMetricsMappingTest
    : public testing::TestWithParam<MappingTestCase> {};

TEST_P(SessionRestoreMetricsMappingTest, Evaluate) {
  const MappingTestCase& test_case = GetParam();
  const SessionRestoreMetrics metrics = EvaluateSessionRestore(
      test_case.status, test_case.replay_result, test_case.any_windows_left);

  EXPECT_EQ(test_case.expected_outcome, metrics.outcome);
  EXPECT_EQ(test_case.expected_reason, metrics.error_reason);
  // An error reason is recorded if and only if the restore failed.
  EXPECT_EQ(metrics.outcome == SessionRestoreOutcome::kFailed,
            metrics.error_reason.has_value());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SessionRestoreMetricsMappingTest,
    testing::ValuesIn(kMappingTestCases),
    [](const testing::TestParamInfo<MappingTestCase>& info) {
      return std::string(info.param.name);
    });

struct ReadStatusTestCase {
  const char* name;
  CommandStorageReadStatus status;
  SessionRestoreOutcome expected_outcome;
  std::optional<SessionRestoreErrorReason> expected_reason;
};

// Every CommandStorageReadStatus value must map to something. The replay result
// is irrelevant for the error rows, because a read-stage error outranks it.
const ReadStatusTestCase kReadStatusTestCases[] = {
    {"Unknown", CommandStorageReadStatus::kUnknown,
     SessionRestoreOutcome::kFailed,
     SessionRestoreErrorReason::kCorruptedOrIncompleteFile},
    {"Success", CommandStorageReadStatus::kSuccess,
     SessionRestoreOutcome::kSuccess, std::nullopt},
    {"NoFile", CommandStorageReadStatus::kNoFile,
     SessionRestoreOutcome::kNoSessionFile, std::nullopt},
    {"FileInvalid", CommandStorageReadStatus::kFileInvalid,
     SessionRestoreOutcome::kFailed,
     SessionRestoreErrorReason::kCorruptedOrIncompleteFile},
    {"FileEmpty", CommandStorageReadStatus::kFileEmpty,
     SessionRestoreOutcome::kFailed,
     SessionRestoreErrorReason::kCorruptedOrIncompleteFile},
    {"InvalidHeader", CommandStorageReadStatus::kInvalidHeader,
     SessionRestoreOutcome::kFailed,
     SessionRestoreErrorReason::kInvalidHeaderOrSignature},
    {"InvalidCommand", CommandStorageReadStatus::kInvalidCommand,
     SessionRestoreOutcome::kFailed,
     SessionRestoreErrorReason::kCorruptedOrIncompleteFile},
    {"UnsupportedVersion", CommandStorageReadStatus::kUnsupportedVersion,
     SessionRestoreOutcome::kFailed,
     SessionRestoreErrorReason::kUnsupportedFileVersion},
    {"DecryptionUnavailable", CommandStorageReadStatus::kDecryptionUnavailable,
     SessionRestoreOutcome::kFailed,
     SessionRestoreErrorReason::kDecryptionUnavailable},
};

class SessionRestoreMetricsReadStatusTest
    : public testing::TestWithParam<ReadStatusTestCase> {};

TEST_P(SessionRestoreMetricsReadStatusTest, Evaluate) {
  const ReadStatusTestCase& test_case = GetParam();
  // kSuccess/true is the shape a healthy restore has; the error rows must
  // override it because the read stage is the root cause.
  const SessionRestoreMetrics metrics =
      EvaluateSessionRestore(test_case.status, SessionReplayResult::kSuccess,
                             /*any_windows_left=*/true);

  EXPECT_EQ(test_case.expected_outcome, metrics.outcome);
  EXPECT_EQ(test_case.expected_reason, metrics.error_reason);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SessionRestoreMetricsReadStatusTest,
    testing::ValuesIn(kReadStatusTestCases),
    [](const testing::TestParamInfo<ReadStatusTestCase>& info) {
      return std::string(info.param.name);
    });

// The read-status cases above pair each status with a healthy replay result.
// This is the case where the two stages genuinely disagree: a truncated file
// returns the commands read before the error along with kInvalidCommand, and
// SessionServiceBase replays that partial stream anyway. Replaying it fails on
// its own terms, but the truncated read is the root cause and must be the
// reported one.
TEST(SessionRestoreMetricsTest, ReadErrorOutranksReplayFailure) {
  for (const SessionReplayResult replay_result :
       {SessionReplayResult::kCorruptedCommand,
        SessionReplayResult::kAllTabsPrunedNoNavigations,
        SessionReplayResult::kAllWindowsHadNoValidTabs}) {
    const SessionRestoreMetrics metrics =
        EvaluateSessionRestore(CommandStorageReadStatus::kInvalidCommand,
                               replay_result, /*any_windows_left=*/false);

    EXPECT_EQ(SessionRestoreOutcome::kFailed, metrics.outcome);
    EXPECT_EQ(SessionRestoreErrorReason::kCorruptedOrIncompleteFile,
              metrics.error_reason);
  }
}

TEST(SessionRestoreMetricsTest, RecordsNormalVariantOnSuccess) {
  base::HistogramTester histogram_tester;

  RecordSessionRestoreMetrics(/*for_apps=*/false,
                              CommandStorageReadStatus::kSuccess,
                              SessionReplayResult::kSuccess,
                              /*any_windows_left=*/true);

  histogram_tester.ExpectUniqueSample("SessionRestore.Outcome.Normal",
                                      SessionRestoreOutcome::kSuccess, 1);
  histogram_tester.ExpectTotalCount("SessionRestore.Outcome.App", 0);
  // No error reason is emitted when the restore succeeded.
  histogram_tester.ExpectTotalCount("SessionRestore.ErrorReason.Normal", 0);
}

TEST(SessionRestoreMetricsTest, RecordsAppVariantWithErrorReason) {
  base::HistogramTester histogram_tester;

  RecordSessionRestoreMetrics(/*for_apps=*/true,
                              CommandStorageReadStatus::kDecryptionUnavailable,
                              SessionReplayResult::kNoCommands,
                              /*any_windows_left=*/false);

  histogram_tester.ExpectUniqueSample("SessionRestore.Outcome.App",
                                      SessionRestoreOutcome::kFailed, 1);
  histogram_tester.ExpectUniqueSample(
      "SessionRestore.ErrorReason.App",
      SessionRestoreErrorReason::kDecryptionUnavailable, 1);
  histogram_tester.ExpectTotalCount("SessionRestore.Outcome.Normal", 0);
}

TEST(SessionRestoreMetricsTest, NoErrorReasonWhenNoSessionFile) {
  base::HistogramTester histogram_tester;

  RecordSessionRestoreMetrics(/*for_apps=*/false,
                              CommandStorageReadStatus::kNoFile,
                              SessionReplayResult::kNoCommands,
                              /*any_windows_left=*/false);

  histogram_tester.ExpectUniqueSample("SessionRestore.Outcome.Normal",
                                      SessionRestoreOutcome::kNoSessionFile, 1);
  histogram_tester.ExpectTotalCount("SessionRestore.ErrorReason.Normal", 0);
}

}  // namespace
