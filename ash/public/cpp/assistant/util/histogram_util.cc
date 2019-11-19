// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/assistant/util/histogram_util.h"

#include "ash/public/cpp/assistant/proactive_suggestions.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"

namespace ash {
namespace assistant {
namespace metrics {

void RecordProactiveSuggestionsRequestResult(
    int category,
    ProactiveSuggestionsRequestResult result) {
  constexpr char kRequestResultHistogram[] =
      "Assistant.ProactiveSuggestions.RequestResult";

  // We record an aggregate histogram for easily reporting cumulative request
  // results across all content categories.
  base::UmaHistogramEnumeration(kRequestResultHistogram, result);

  // We record sparse histograms for easily comparing request results between
  // content categories.
  switch (result) {
    case ProactiveSuggestionsRequestResult::kError:
      base::UmaHistogramSparse(
          base::StringPrintf("%s.Error", kRequestResultHistogram), category);
      break;
    case ProactiveSuggestionsRequestResult::kSuccessWithContent:
      base::UmaHistogramSparse(
          base::StringPrintf("%s.SuccessWithContent", kRequestResultHistogram),
          category);
      break;
    case ProactiveSuggestionsRequestResult::kSuccessWithoutContent:
      base::UmaHistogramSparse(base::StringPrintf("%s.SuccessWithoutContent",
                                                  kRequestResultHistogram),
                               category);
      break;
  }
}

void RecordProactiveSuggestionsShowAttempt(
    int category,
    ProactiveSuggestionsShowAttempt attempt) {
  constexpr char kShowAttemptHistogram[] =
      "Assistant.ProactiveSuggestions.ShowAttempt";

  // We record an aggregate histogram for easily reporting cumulative show
  // attempts across all content categories.
  base::UmaHistogramEnumeration(kShowAttemptHistogram, attempt);

  // We record sparse histograms for easily comparing show attempts between
  // content categories.
  switch (attempt) {
    case ProactiveSuggestionsShowAttempt::kSuccess:
      base::UmaHistogramSparse(
          base::StringPrintf("%s.Success", kShowAttemptHistogram), category);
      break;
    case ProactiveSuggestionsShowAttempt::kAbortedByDuplicateSuppression:
      base::UmaHistogramSparse(
          base::StringPrintf("%s.AbortedByDuplicateSuppression",
                             kShowAttemptHistogram),
          category);
      break;
  }
}

void RecordProactiveSuggestionsShowResult(
    int category,
    ProactiveSuggestionsShowResult result) {
  constexpr char kShowResultHistogram[] =
      "Assistant.ProactiveSuggestions.ShowResult";

  // We record an aggregate histogram for easily reporting cumulative show
  // results across all content categories.
  base::UmaHistogramEnumeration(kShowResultHistogram, result);

  // We record sparse histograms for easily comparing show results between
  // content categories.
  switch (result) {
    case ProactiveSuggestionsShowResult::kClick:
      base::UmaHistogramSparse(
          base::StringPrintf("%s.Click", kShowResultHistogram), category);
      break;
    case ProactiveSuggestionsShowResult::kCloseByContextChange:
      base::UmaHistogramSparse(
          base::StringPrintf("%s.CloseByContextChange", kShowResultHistogram),
          category);
      break;
    case ProactiveSuggestionsShowResult::kCloseByTimeout:
      base::UmaHistogramSparse(
          base::StringPrintf("%s.CloseByTimeout", kShowResultHistogram),
          category);
      break;
    case ProactiveSuggestionsShowResult::kCloseByUser:
      base::UmaHistogramSparse(
          base::StringPrintf("%s.CloseByUser", kShowResultHistogram), category);
      break;
  }
}

}  // namespace metrics
}  // namespace assistant
}  // namespace ash
