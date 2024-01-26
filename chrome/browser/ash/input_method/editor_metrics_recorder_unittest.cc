// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/editor_metrics_recorder.h"

#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/input_method/editor_consent_enums.h"
#include "chrome/browser/ash/input_method/editor_metrics_enums.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::input_method {

namespace {

class EditorMetricsRecorderTest : public testing::Test {
 public:
  EditorMetricsRecorderTest() = default;
  ~EditorMetricsRecorderTest() override = default;
  base::HistogramTester histogram_tester_;

 private:
  content::BrowserTaskEnvironment task_environment_;
};

struct StateCase {
  std::string test_name;
  EditorOpportunityMode mode;
  EditorTone tone;
  EditorStates state;
  std::string histogram_name;
};

class StateRewriteMetricsTest : public EditorMetricsRecorderTest,
                                public testing::WithParamInterface<StateCase> {
};

TEST_P(StateRewriteMetricsTest, RecordStateMetricPerTone) {
  const StateCase& test_case = GetParam();
  EditorMetricsRecorder metrics_recorder(test_case.mode);
  metrics_recorder.SetTone(test_case.tone);

  metrics_recorder.LogEditorState(test_case.state);

  histogram_tester_.ExpectUniqueSample("InputMethod.Manta.Orca.States.Rewrite",
                                       test_case.state, 1);
  histogram_tester_.ExpectUniqueSample(test_case.histogram_name,
                                       test_case.state, 1);
}

INSTANTIATE_TEST_SUITE_P(
    EditorMetricsRecorderTest,
    StateRewriteMetricsTest,
    testing::ValuesIn<StateCase>({
        {"OpportunityRewrite", EditorOpportunityMode::kRewrite,
         EditorTone::kUnset, EditorStates::kNativeUIShowOpportunity,
         /*histogram_name=*/"InputMethod.Manta.Orca.States.Rewrite"},
        {"NativeUIShownRewrite", EditorOpportunityMode::kRewrite,
         EditorTone::kUnset, EditorStates::kNativeUIShown,
         /*histogram_name=*/"InputMethod.Manta.Orca.States.Rewrite"},
        {"NativeRequestRephrase", EditorOpportunityMode::kRewrite,
         EditorTone::kRephrase, EditorStates::kNativeRequest,
         /*histogram_name=*/"InputMethod.Manta.Orca.States.Rephrase"},
        {"InsertEmojify", EditorOpportunityMode::kRewrite, EditorTone::kEmojify,
         EditorStates::kInsert,
         /*histogram_name=*/"InputMethod.Manta.Orca.States.Emojify"},
        {"ClickCloseButtonShorten", EditorOpportunityMode::kRewrite,
         EditorTone::kShorten, EditorStates::kClickCloseButton,
         /*histogram_name=*/"InputMethod.Manta.Orca.States.Shorten"},
        {"ApproveConsentElaborate", EditorOpportunityMode::kRewrite,
         EditorTone::kElaborate, EditorStates::kApproveConsent,
         /*histogram_name=*/"InputMethod.Manta.Orca.States.Elaborate"},
        {"DeclineConsentFormalize", EditorOpportunityMode::kRewrite,
         EditorTone::kFormalize, EditorStates::kDeclineConsent,
         /*histogram_name=*/"InputMethod.Manta.Orca.States.Formalize"},
        {"NativeRequestFreeformRewrite", EditorOpportunityMode::kRewrite,
         EditorTone::kFreeformRewrite, EditorStates::kNativeRequest,
         /*histogram_name=*/"InputMethod.Manta.Orca.States.FreeformRewrite"},

    }),
    [](const testing::TestParamInfo<StateCase> info) {
      return info.param.test_name;
    });

class StateWriteMetricsTest : public EditorMetricsRecorderTest,
                              public testing::WithParamInterface<StateCase> {};

TEST_P(StateWriteMetricsTest, RecordStateMetricPerTone) {
  const StateCase& test_case = GetParam();
  EditorMetricsRecorder metrics_recorder(test_case.mode);
  metrics_recorder.SetTone(test_case.tone);

  metrics_recorder.LogEditorState(test_case.state);

  histogram_tester_.ExpectUniqueSample(test_case.histogram_name,
                                       test_case.state, 1);
}

INSTANTIATE_TEST_SUITE_P(
    EditorMetricsRecorderTest,
    StateWriteMetricsTest,
    testing::ValuesIn<StateCase>({
        {"OpportunityWrite", EditorOpportunityMode::kWrite, EditorTone::kUnset,
         EditorStates::kNativeUIShowOpportunity,
         /*histogram_name=*/"InputMethod.Manta.Orca.States.Write"},
        {"NativeUIShownWrite", EditorOpportunityMode::kWrite,
         EditorTone::kUnset, EditorStates::kNativeUIShown,
         /*histogram_name=*/"InputMethod.Manta.Orca.States.Write"},
        {"NativeRequestWrite", EditorOpportunityMode::kWrite,
         EditorTone::kUnset, EditorStates::kNativeRequest,
         /*histogram_name=*/"InputMethod.Manta.Orca.States.Write"},
    }),
    [](const testing::TestParamInfo<StateCase> info) {
      return info.param.test_name;
    });

struct CharectersInsertedCase {
  std::string test_name;
  EditorOpportunityMode mode;
  EditorTone tone;
  int number_of_characters;
  std::string tone_string;
};

class CharectersInsertedMetricsTest
    : public EditorMetricsRecorderTest,
      public testing::WithParamInterface<CharectersInsertedCase> {};

TEST_P(CharectersInsertedMetricsTest, RecordStateMetricPerTone) {
  const CharectersInsertedCase& test_case = GetParam();
  EditorMetricsRecorder metrics_recorder(test_case.mode);
  metrics_recorder.SetTone(test_case.tone);

  metrics_recorder.LogNumberOfCharactersInserted(
      test_case.number_of_characters);
  metrics_recorder.LogNumberOfCharactersSelectedForInsert(
      test_case.number_of_characters);

  histogram_tester_.ExpectTotalCount(
      "InputMethod.Manta.Orca.CharactersInserted.Rewrite",
      test_case.number_of_characters);
  histogram_tester_.ExpectTotalCount(
      "InputMethod.Manta.Orca.CharactersSelectedForInsert.Rewrite",
      test_case.number_of_characters);
  histogram_tester_.ExpectTotalCount(
      base::StrCat({"InputMethod.Manta.Orca.CharactersInserted.",
                    test_case.tone_string}),
      test_case.number_of_characters);
  histogram_tester_.ExpectTotalCount(
      base::StrCat({"InputMethod.Manta.Orca.CharactersSelectedForInsert.",
                    test_case.tone_string}),
      test_case.number_of_characters);
}

INSTANTIATE_TEST_SUITE_P(
    EditorMetricsRecorderTest,
    CharectersInsertedMetricsTest,
    testing::ValuesIn<CharectersInsertedCase>({
        {"Rephrase", EditorOpportunityMode::kRewrite, EditorTone::kRephrase,
         /*number_of_characters=*/1,
         /*tone_string=*/"Rephrase"},
        {"Emojify", EditorOpportunityMode::kRewrite, EditorTone::kEmojify,
         /*number_of_characters=*/1,
         /*tone_string=*/"Emojify"},
        {"Shorten", EditorOpportunityMode::kRewrite, EditorTone::kShorten,
         /*number_of_characters=*/1,
         /*tone_string=*/"Shorten"},
        {"Elaborate", EditorOpportunityMode::kRewrite, EditorTone::kElaborate,
         /*number_of_characters=*/1,
         /*tone_string=*/"Elaborate"},
        {"Formalize", EditorOpportunityMode::kRewrite, EditorTone::kFormalize,
         /*number_of_characters=*/1,
         /*tone_string=*/"Formalize"},
        {"FreeformRewrite", EditorOpportunityMode::kRewrite,
         EditorTone::kFreeformRewrite,
         /*number_of_characters=*/1,
         /*tone_string=*/"FreeformRewrite"},

    }),
    [](const testing::TestParamInfo<CharectersInsertedCase> info) {
      return info.param.test_name;
    });

TEST_F(EditorMetricsRecorderTest, WriteCharectersInsertedMetrics) {
  EditorMetricsRecorder metrics_recorder(EditorOpportunityMode::kWrite);
  metrics_recorder.SetTone(EditorTone::kUnset);

  metrics_recorder.LogNumberOfCharactersInserted(1);
  metrics_recorder.LogNumberOfCharactersSelectedForInsert(1);

  histogram_tester_.ExpectTotalCount(
      base::StrCat({"InputMethod.Manta.Orca.CharactersInserted.", "Write"}), 1);
  histogram_tester_.ExpectTotalCount(
      base::StrCat(
          {"InputMethod.Manta.Orca.CharactersSelectedForInsert.", "Write"}),
      1);
}

struct SetToneCase {
  std::string test_name;
  std::optional<std::string_view> query_tone_string;
  std::optional<std::string_view> freeform_text;
  std::string expected_tone_string;
};

class SettingToneFromQueryAndFreeformTest
    : public EditorMetricsRecorderTest,
      public testing::WithParamInterface<SetToneCase> {};

TEST_P(SettingToneFromQueryAndFreeformTest, ConvertQueryToneToMetricTone) {
  const SetToneCase& test_case = GetParam();
  EditorMetricsRecorder metrics_recorder(EditorOpportunityMode::kRewrite);
  metrics_recorder.SetTone(test_case.query_tone_string,
                           test_case.freeform_text);

  metrics_recorder.LogEditorState(EditorStates::kNativeRequest);

  histogram_tester_.ExpectUniqueSample(
      base::StrCat(
          {"InputMethod.Manta.Orca.States.", test_case.expected_tone_string}),
      EditorStates::kNativeRequest, 1);
}

INSTANTIATE_TEST_SUITE_P(EditorMetricsRecorderTest,
                         SettingToneFromQueryAndFreeformTest,
                         testing::ValuesIn<SetToneCase>({
                             {"Unset",
                              /*query_tone_string=*/std::nullopt,
                              /*freeform_text=*/std::nullopt,
                              /*tone_string=*/"Unset"},
                             {"Rephrase",
                              /*query_tone_string=*/"REPHRASE",
                              /*freeform_text=*/std::nullopt,
                              /*tone_string=*/"Rephrase"},
                             {"Emojify",
                              /*query_tone_string=*/"EMOJIFY",
                              /*freeform_text=*/std::nullopt,
                              /*tone_string=*/"Emojify"},
                             {"Shorten",
                              /*query_tone_string=*/"SHORTEN",
                              /*freeform_text=*/std::nullopt,
                              /*tone_string=*/"Shorten"},
                             {"Elaborate",
                              /*query_tone_string=*/"ELABORATE",
                              /*freeform_text=*/std::nullopt,
                              /*tone_string=*/"Elaborate"},
                             {"Formalize",
                              /*query_tone_string=*/"FORMALIZE",
                              /*freeform_text=*/std::nullopt,
                              /*tone_string=*/"Formalize"},
                             {"FreeformRewrite",
                              /*query_tone_string=*/std::nullopt,
                              /*freeform_text=*/"write me a story",
                              /*tone_string=*/"FreeformRewrite"},
                             {"Unknown",
                              /*query_tone_string=*/"RANDOM",
                              /*freeform_text=*/std::nullopt,
                              /*tone_string=*/"Unknown"},
                         }),
                         [](const testing::TestParamInfo<SetToneCase> info) {
                           return info.param.test_name;
                         });

}  // namespace
}  // namespace ash::input_method
