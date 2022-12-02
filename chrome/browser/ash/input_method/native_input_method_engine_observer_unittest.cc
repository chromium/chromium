// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/native_input_method_engine_observer.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::input_method {
namespace {

class NativeInputMethodEngineObserverTest : public ::testing::Test {
 public:
  NativeInputMethodEngineObserverTest()
      : observer_(/*prefs=*/profile_.GetPrefs(),
                  /*ime_base_observer=*/nullptr,
                  /*assistive_suggester=*/nullptr,
                  /*autocorrect_manager=*/nullptr,
                  /*suggestions_collector=*/nullptr,
                  /*grammar_manager=*/nullptr,
                  /*use_ime_service=*/false) {}

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  NativeInputMethodEngineObserver observer_;
};

TEST_F(NativeInputMethodEngineObserverTest,
       RecordsSuggestionOpportunityForCompletions) {
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.MultiWord.SuggestionOpportunity", 0);

  observer_.ReportSuggestionOpportunity(
      ime::AssistiveSuggestionMode::kCompletion);

  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.MultiWord.SuggestionOpportunity", 1);
  histogram_tester.ExpectUniqueSample(
      "InputMethod.Assistive.MultiWord.SuggestionOpportunity",
      /*sample=*/MultiWordSuggestionType::kCompletion,
      /*expected_bucket_count=*/1);
}

TEST_F(NativeInputMethodEngineObserverTest,
       RecordsSuggestionOpportunityForPredictions) {
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.MultiWord.SuggestionOpportunity", 0);

  observer_.ReportSuggestionOpportunity(
      ime::AssistiveSuggestionMode::kPrediction);

  histogram_tester.ExpectTotalCount(
      "InputMethod.Assistive.MultiWord.SuggestionOpportunity", 1);
  histogram_tester.ExpectUniqueSample(
      "InputMethod.Assistive.MultiWord.SuggestionOpportunity",
      /*sample=*/MultiWordSuggestionType::kPrediction,
      /*expected_bucket_count=*/1);
}

}  // namespace
}  // namespace ash::input_method
