// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/selection/metrics_logger.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/selection/mojom/action.mojom.h"
#include "chrome/browser/selection/suggestion.h"
#include "components/optimization_guide/proto/features/smart_selection_suggestions.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace selection {
namespace {

using ::base::Bucket;
using ::base::BucketsAre;

class FakeSuggestion : public Suggestion {
 public:
  explicit FakeSuggestion(ToolId tool_id) : tool_id_(tool_id) {}
  ~FakeSuggestion() override = default;

  const std::u16string& GetLabel() const override { return label_; }
  ToolId GetToolId() const override { return tool_id_; }
  void OnSuggestionPresented() override {}
  void OnSuggestionExecuted() override {}
  mojom::ActionPtr GetAction() const override {
    return mojom::Action::NewHandoff(mojom::Handoff::New());
  }

 private:
  const std::u16string label_ = u"label";
  const ToolId tool_id_;
};

TEST(MetricsLoggerTest, PreselectedRegionLogsImmediately) {
  base::HistogramTester histogram_tester;
  {
    MetricsLogger logger(EntryPoint::kContextMenuText, "application/pdf",
                         /*has_preselected_region=*/true);
    logger.RecordSessionEnd(/*cancelled_by_user=*/true);
  }

  histogram_tester.ExpectUniqueSample("SmartSelection.Invocation.ByEntryPoint",
                                      EntryPoint::kContextMenuText, 1);
  histogram_tester.ExpectUniqueSample(
      "SmartSelection.Invocation.ByDocumentMimeType", DocumentMimeType::kPdf,
      1);
  histogram_tester.ExpectUniqueSample("SmartSelection.Selection.Outcome",
                                      SelectionOutcome::kPreselectedText, 1);
}

TEST(MetricsLoggerTest, SpatialSelectionCompleted) {
  base::HistogramTester histogram_tester;
  {
    MetricsLogger logger(EntryPoint::kKeyboardShortcut, "text/html",
                         /*has_preselected_region=*/false);
    logger.RecordSpatialSelectionCompleted();
    logger.RecordSpatialSelectionCompleted();
  }

  histogram_tester.ExpectUniqueSample(
      "SmartSelection.Selection.Outcome",
      SelectionOutcome::kSpatialSelectionCompleted, 1);
}

TEST(MetricsLoggerTest, CancelledAtSelectionStage) {
  base::HistogramTester histogram_tester;
  {
    MetricsLogger logger(EntryPoint::kKeyboardShortcut, "text/html",
                         /*has_preselected_region=*/false);
    logger.RecordSessionEnd(/*cancelled_by_user=*/true);
  }

  histogram_tester.ExpectUniqueSample("SmartSelection.Selection.Outcome",
                                      SelectionOutcome::kCancelledByUser, 1);
  histogram_tester.ExpectTotalCount("SmartSelection.Submission.Outcome", 0);
}

TEST(MetricsLoggerTest, AbandonedAtSelectionStage) {
  base::HistogramTester histogram_tester;
  {
    MetricsLogger logger(EntryPoint::kKeyboardShortcut, "text/html",
                         /*has_preselected_region=*/false);
  }

  histogram_tester.ExpectUniqueSample("SmartSelection.Selection.Outcome",
                                      SelectionOutcome::kAbandonedOrError, 1);
  histogram_tester.ExpectTotalCount("SmartSelection.Submission.Outcome", 0);
}

TEST(MetricsLoggerTest, SubmissionOutcomes) {
  const FakeSuggestion suggestion(
      optimization_guide::proto::SMART_SELECTION_TOOL_GOOGLE_SEARCH);
  {
    base::HistogramTester histogram_tester;
    MetricsLogger logger(EntryPoint::kContextMenuText, "text/html",
                         /*has_preselected_region=*/true);
    logger.RecordPromptSubmitted();
    logger.RecordSessionEnd(/*cancelled_by_user=*/false);
    histogram_tester.ExpectUniqueSample("SmartSelection.Submission.Outcome",
                                        SubmissionOutcome::kPromptSubmitted, 1);
  }

  {
    base::HistogramTester histogram_tester;
    MetricsLogger logger(EntryPoint::kContextMenuText, "text/html",
                         /*has_preselected_region=*/true);
    logger.RecordSuggestionAccepted(suggestion);
    logger.RecordSessionEnd(/*cancelled_by_user=*/false);
    histogram_tester.ExpectUniqueSample("SmartSelection.Submission.Outcome",
                                        SubmissionOutcome::kSuggestionAccepted,
                                        1);
  }

  {
    base::HistogramTester histogram_tester;
    MetricsLogger logger(EntryPoint::kContextMenuText, "text/html",
                         /*has_preselected_region=*/true);
    logger.RecordSuggestionAccepted(suggestion);
    logger.RecordSuggestionAccepted(suggestion);
    logger.RecordSessionEnd(/*cancelled_by_user=*/false);
    histogram_tester.ExpectUniqueSample("SmartSelection.Submission.Outcome",
                                        SubmissionOutcome::kMultiple, 1);
  }

  {
    base::HistogramTester histogram_tester;
    MetricsLogger logger(EntryPoint::kContextMenuText, "text/html",
                         /*has_preselected_region=*/true);
    logger.RecordSuggestionAccepted(suggestion);
    logger.RecordPromptSubmitted();
    logger.RecordSessionEnd(/*cancelled_by_user=*/false);
    histogram_tester.ExpectUniqueSample("SmartSelection.Submission.Outcome",
                                        SubmissionOutcome::kMultiple, 1);
  }

  {
    base::HistogramTester histogram_tester;
    MetricsLogger logger(EntryPoint::kContextMenuText, "text/html",
                         /*has_preselected_region=*/true);
    logger.RecordSessionEnd(/*cancelled_by_user=*/true);
    histogram_tester.ExpectUniqueSample("SmartSelection.Submission.Outcome",
                                        SubmissionOutcome::kCancelledByUser, 1);
  }

  {
    base::HistogramTester histogram_tester;
    MetricsLogger logger(EntryPoint::kContextMenuText, "text/html",
                         /*has_preselected_region=*/true);
    logger.RecordSessionEnd(/*cancelled_by_user=*/false);
    histogram_tester.ExpectUniqueSample("SmartSelection.Submission.Outcome",
                                        SubmissionOutcome::kAbandonedOrError,
                                        1);
  }
}

TEST(MetricsLoggerTest, RecordSuggestionAcceptedLogsToolType) {
  base::HistogramTester histogram_tester;
  MetricsLogger logger(EntryPoint::kContextMenuText, "text/html",
                       /*has_preselected_region=*/true);

  const FakeSuggestion gemini(
      optimization_guide::proto::SMART_SELECTION_TOOL_GEMINI_IN_CHROME);
  const FakeSuggestion lens(
      optimization_guide::proto::SMART_SELECTION_TOOL_GOOGLE_LENS);
  const FakeSuggestion search(
      optimization_guide::proto::SMART_SELECTION_TOOL_GOOGLE_SEARCH);
  const FakeSuggestion qa(
      optimization_guide::proto::SMART_SELECTION_TOOL_QUICK_ANSWERS);
  const FakeSuggestion unspecified(
      optimization_guide::proto::SMART_SELECTION_TOOL_UNSPECIFIED);

  logger.RecordSuggestionAccepted(gemini);
  logger.RecordSuggestionAccepted(lens);
  logger.RecordSuggestionAccepted(search);
  logger.RecordSuggestionAccepted(qa);
  logger.RecordSuggestionAccepted(unspecified);

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "SmartSelection.Suggestions.AcceptedSuggestionType"),
      BucketsAre(
          Bucket(optimization_guide::proto::SMART_SELECTION_TOOL_UNSPECIFIED,
                 1),
          Bucket(
              optimization_guide::proto::SMART_SELECTION_TOOL_GEMINI_IN_CHROME,
              1),
          Bucket(optimization_guide::proto::SMART_SELECTION_TOOL_GOOGLE_LENS,
                 1),
          Bucket(optimization_guide::proto::SMART_SELECTION_TOOL_GOOGLE_SEARCH,
                 1),
          Bucket(optimization_guide::proto::SMART_SELECTION_TOOL_QUICK_ANSWERS,
                 1)));
}

TEST(MetricsLoggerTest, RecordSuggestionsShownCounts) {
  base::HistogramTester histogram_tester;
  MetricsLogger logger(EntryPoint::kContextMenuText, "text/html",
                       /*has_preselected_region=*/true);

  std::vector<std::unique_ptr<Suggestion>> suggestions;
  suggestions.push_back(std::make_unique<FakeSuggestion>(
      optimization_guide::proto::SMART_SELECTION_TOOL_GEMINI_IN_CHROME));
  suggestions.push_back(std::make_unique<FakeSuggestion>(
      optimization_guide::proto::SMART_SELECTION_TOOL_GEMINI_IN_CHROME));
  suggestions.push_back(std::make_unique<FakeSuggestion>(
      optimization_guide::proto::SMART_SELECTION_TOOL_GOOGLE_LENS));
  suggestions.push_back(std::make_unique<FakeSuggestion>(
      optimization_guide::proto::SMART_SELECTION_TOOL_GOOGLE_SEARCH));
  logger.RecordSuggestionsShown(suggestions);

  histogram_tester.ExpectUniqueSample(
      "SmartSelection.Suggestions.TotalShownCount", 4, 1);
  histogram_tester.ExpectUniqueSample(
      "SmartSelection.Suggestions.GeminiInChromeShownCount", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "SmartSelection.Suggestions.GoogleLensShownCount", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "SmartSelection.Suggestions.GoogleSearchShownCount", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "SmartSelection.Suggestions.QuickAnswersShownCount", 0, 1);
  histogram_tester.ExpectUniqueSample(
      "SmartSelection.Suggestions.UnknownShownCount", 0, 1);
}

}  // namespace
}  // namespace selection
