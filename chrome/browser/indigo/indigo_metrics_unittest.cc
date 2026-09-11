// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/indigo/indigo_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/types/expected.h"
#include "chrome/browser/indigo/indigo_service.h"
#include "chrome/browser/ui/page_action/page_action_controller.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace indigo {
namespace {

TEST(IndigoMetricsTest, RecordShownEntryPoint) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  RecordShownEntryPoint(IndigoPageActionEntryPoint::kSuggestionChip);

  histogram_tester.ExpectUniqueSample(
      kShownEntryPointHistogram, IndigoPageActionEntryPoint::kSuggestionChip,
      1);
  EXPECT_EQ(1, user_action_tester.GetActionCount(kSuggestionChipShowAction));
}

TEST(IndigoMetricsTest, RecordClickedEntryPoint_SuggestionChip) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  RecordClickedEntryPoint(EntryPoint::kSuggestionChip, std::nullopt);

  histogram_tester.ExpectUniqueSample(
      kClickedEntryPointHistogram, IndigoPageActionEntryPoint::kSuggestionChip,
      1);
  EXPECT_EQ(1, user_action_tester.GetActionCount(kSuggestionChipClickAction));
}

TEST(IndigoMetricsTest, RecordClickedEntryPoint_AnchoredMessageContextualCue) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  RecordClickedEntryPoint(
      EntryPoint::kAnchoredMessage,
      page_actions::PageActionPriorityCategory::kContextualCue);

  histogram_tester.ExpectUniqueSample(
      kClickedEntryPointHistogram,
      IndigoPageActionEntryPoint::kProactiveAnchoredMessage, 1);
  EXPECT_EQ(1, user_action_tester.GetActionCount(kAnchoredMessageClickAction));
}

TEST(IndigoMetricsTest,
     RecordClickedEntryPoint_AnchoredMessageUserInteraction) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  RecordClickedEntryPoint(
      EntryPoint::kAnchoredMessage,
      page_actions::PageActionPriorityCategory::kUserInteraction);

  histogram_tester.ExpectUniqueSample(
      kClickedEntryPointHistogram,
      IndigoPageActionEntryPoint::kReactiveAnchoredMessage, 1);
  EXPECT_EQ(1, user_action_tester.GetActionCount(kAnchoredMessageClickAction));
}

TEST(IndigoMetricsTest, RecordClickedEntryPoint_ErrorToast) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  RecordClickedEntryPoint(EntryPoint::kErrorToast, std::nullopt);

  histogram_tester.ExpectUniqueSample(
      kClickedEntryPointHistogram, IndigoPageActionEntryPoint::kErrorToast, 1);
  EXPECT_EQ(1, user_action_tester.GetActionCount(kErrorToastRetryClickAction));
}

TEST(IndigoMetricsTest, RecordTransformationTrigger) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  RecordTransformationTrigger(IndigoTransformationTriggerSource::kPageAction);

  histogram_tester.ExpectUniqueSample(
      kTransformationTriggerSourceHistogram,
      IndigoTransformationTriggerSource::kPageAction, 1);
  EXPECT_EQ(1, user_action_tester.GetActionCount(kTransformationTriggerAction));
}

TEST(IndigoMetricsTest, RecordTransformationResult_Success) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  RecordTransformationResult(IndigoTransformationResult::kSuccess);

  histogram_tester.ExpectUniqueSample(kTransformationResultHistogram,
                                      IndigoTransformationResult::kSuccess, 1);
  EXPECT_EQ(1, user_action_tester.GetActionCount(kTransformationSuccessAction));
  EXPECT_EQ(0, user_action_tester.GetActionCount(kTransformationFailureAction));
}

TEST(IndigoMetricsTest, RecordTransformationResult_Failure) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  RecordTransformationResult(IndigoTransformationResult::kMissingCapabilities);

  histogram_tester.ExpectUniqueSample(
      kTransformationResultHistogram,
      IndigoTransformationResult::kMissingCapabilities, 1);
  EXPECT_EQ(0, user_action_tester.GetActionCount(kTransformationSuccessAction));
  EXPECT_EQ(1, user_action_tester.GetActionCount(kTransformationFailureAction));
}

TEST(IndigoMetricsTest,
     RecordTransformationResultCannotGenerateImage_LocalEligibility) {
  struct TestCase {
    LocalEligibility local_eligibility;
    IndigoTransformationResult expected_result;
  } test_cases[] = {
      {LocalEligibility::kNotSignedIn,
       IndigoTransformationResult::kNotSignedIn},
      {LocalEligibility::kRefreshTokenInPersistentErrorState,
       IndigoTransformationResult::kRefreshTokenInPersistentErrorState},
      {LocalEligibility::kMissingCapabilities,
       IndigoTransformationResult::kMissingCapabilities},
      {LocalEligibility::kDisabledByPolicy,
       IndigoTransformationResult::kDisabledByPolicy},
      {LocalEligibility::kMissingScript,
       IndigoTransformationResult::kMissingScript},
      {LocalEligibility::kManagedDomain,
       IndigoTransformationResult::kManagedDomain},
      {LocalEligibility::kGlicDisabledForProfile,
       IndigoTransformationResult::kGlicDisabledForProfile},
      {LocalEligibility::kEnterpriseDisallowed,
       IndigoTransformationResult::kEnterpriseDisallowed},
  };

  for (const auto& test_case : test_cases) {
    base::HistogramTester histogram_tester;
    base::UserActionTester user_action_tester;

    CombinedEligibility eligibility;
    eligibility.local_eligibility = test_case.local_eligibility;
    eligibility.remote_eligibility = RemoteEligibility{true, true};
    eligibility.has_onboarded_pref = true;

    RecordTransformationResultCannotGenerateImage(eligibility);

    histogram_tester.ExpectUniqueSample(kTransformationResultHistogram,
                                        test_case.expected_result, 1);
    EXPECT_EQ(1,
              user_action_tester.GetActionCount(kTransformationFailureAction));
  }
}

TEST(IndigoMetricsTest,
     RecordTransformationResultCannotGenerateImage_RemoteEligibility) {
  struct TestCase {
    base::expected<RemoteEligibility, std::string> remote_eligibility;
    bool has_onboarded_pref;
    IndigoTransformationResult expected_result;
  } test_cases[] = {
      {base::unexpected("error"), true,
       IndigoTransformationResult::kRemoteStatusMissing},
      {RemoteEligibility{false, true}, true,
       IndigoTransformationResult::kServiceNotSupported},
      {RemoteEligibility{true, false}, true,
       IndigoTransformationResult::kMissingUserImage},
      {RemoteEligibility{true, true}, false,
       IndigoTransformationResult::kNotOnboarded},
  };

  for (const auto& test_case : test_cases) {
    base::HistogramTester histogram_tester;
    base::UserActionTester user_action_tester;

    CombinedEligibility eligibility;
    eligibility.local_eligibility = LocalEligibility::kEligible;
    eligibility.remote_eligibility = test_case.remote_eligibility;
    eligibility.has_onboarded_pref = test_case.has_onboarded_pref;

    RecordTransformationResultCannotGenerateImage(eligibility);

    histogram_tester.ExpectUniqueSample(kTransformationResultHistogram,
                                        test_case.expected_result, 1);
    EXPECT_EQ(1,
              user_action_tester.GetActionCount(kTransformationFailureAction));
  }
}

}  // namespace
}  // namespace indigo
