// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/editor_metrics_recorder.h"

#include <optional>

#include "ash/constants/ash_features.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/input_method/editor_consent_enums.h"
#include "chrome/browser/ash/input_method/editor_context.h"
#include "chrome/browser/ash/input_method/editor_geolocation_mock_provider.h"
#include "chrome/browser/ash/input_method/editor_metrics_enums.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_task_environment.h"
#include "editor_metrics_enums.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/metrics/public/mojom/ukm_interface.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::input_method {

namespace {

using base::test::ScopedFeatureList;
using testing::IsEmpty;
using UkmEntry = ukm::builders::InputMethod_Manta_Orca;

constexpr std::string_view kAllowedCountryCode = "au";

class FakeContextObserver : public EditorContext::Observer {
 public:
  FakeContextObserver() = default;
  ~FakeContextObserver() override = default;

  // EditorContext::Observer overrides
  void OnContextUpdated() override {}
  void OnImeChange(std::string_view engine_id) override {}
};

class FakeSystem : public EditorContext::System {
 public:
  FakeSystem() {}

  explicit FakeSystem(ukm::SourceId ukm_source_id)
      : ukm_source_id_(ukm_source_id) {}

  ~FakeSystem() override = default;

  // EditorContext::System overrides
  std::optional<ukm::SourceId> GetUkmSourceId() override {
    return ukm_source_id_;
  }

 private:
  std::optional<ukm::SourceId> ukm_source_id_;
};

class EditorMetricsRecorderTest : public testing::Test {
 public:
  EditorMetricsRecorderTest() = default;
  ~EditorMetricsRecorderTest() override = default;
  base::HistogramTester histogram_tester_;

 private:
  content::BrowserTaskEnvironment task_environment_;
};

class EditorStateMetrics : public EditorMetricsRecorderTest,
                           public testing::WithParamInterface<EditorStates> {};

INSTANTIATE_TEST_SUITE_P(EditorMetricsRecorderTest,
                         EditorStateMetrics,
                         testing::ValuesIn<EditorStates>({
                             EditorStates::kNativeUIShown,
                             EditorStates::kPromoCardImpression,
                             EditorStates::kNativeRequest,
                             EditorStates::kWebUIRequest,
                             EditorStates::kInsert,
                             EditorStates::kNativeUIShowOpportunity,
                             EditorStates::kDismiss,
                             EditorStates::kRefineRequest,
                             EditorStates::kSuccessResponse,
                             EditorStates::kErrorResponse,
                             EditorStates::kThumbsUp,
                             EditorStates::kThumbsDown,
                             EditorStates::kReturnToPreviousSuggestions,
                             EditorStates::kClickCloseButton,
                             EditorStates::kApproveConsent,
                             EditorStates::kDeclineConsent,
                             EditorStates::kBlocked,
                             EditorStates::kBlockedByUnsupportedRegion,
                             EditorStates::kBlockedByConsent,
                             EditorStates::kBlockedBySetting,
                             EditorStates::kBlockedByTextLength,
                             EditorStates::kBlockedByUrl,
                             EditorStates::kBlockedByApp,
                             EditorStates::kBlockedByInputMethod,
                             EditorStates::kBlockedByInputType,
                             EditorStates::kBlockedByAppType,
                             EditorStates::kBlockedByInvalidFormFactor,
                             EditorStates::kBlockedByNetworkStatus,
                             EditorStates::kErrorUnknown,
                             EditorStates::kErrorInvalidArgument,
                             EditorStates::kErrorResourceExhausted,
                             EditorStates::kErrorBackendFailure,
                             EditorStates::kErrorNoInternetConnection,
                             EditorStates::kErrorUnsupportedLanguage,
                             EditorStates::kErrorBlockedOutputs,
                             EditorStates::kErrorRestrictedRegion,
                             EditorStates::kPromoCardExplicitDismissal,
                             EditorStates::kConsentScreenImpression,
                             EditorStates::kTextInsertionRequested,
                             EditorStates::kTextQueuedForInsertion,
                             EditorStates::kRequest,
                             EditorStates::kBlockedByUnsupportedCapability,
                             EditorStates::kBlockedByUnknownCapability,
                             EditorStates::kBlockedByPolicy,
                         }));

TEST_P(EditorStateMetrics, RecordsForWrite) {
  const EditorStates& state = GetParam();
  FakeSystem system;
  FakeContextObserver observer;
  EditorGeolocationMockProvider geolocation_provider("us");
  EditorContext context(&observer, &system, &geolocation_provider);
  EditorMetricsRecorder metrics_recorder(&context,
                                         EditorOpportunityMode::kWrite);

  metrics_recorder.LogEditorState(state);

  histogram_tester_.ExpectBucketCount("InputMethod.Manta.Orca.States.Write",
                                      state, 1);
  histogram_tester_.ExpectTotalCount("InputMethod.Manta.Orca.States.Write", 1);
}

TEST_P(EditorStateMetrics, RecordsForRewrite) {
  const EditorStates& state = GetParam();
  FakeSystem system;
  FakeContextObserver observer;
  EditorGeolocationMockProvider geolocation_provider(kAllowedCountryCode);
  EditorContext context(&observer, &system, &geolocation_provider);
  EditorMetricsRecorder metrics_recorder(&context,
                                         EditorOpportunityMode::kRewrite);

  metrics_recorder.LogEditorState(state);

  histogram_tester_.ExpectBucketCount("InputMethod.Manta.Orca.States.Rewrite",
                                      state, 1);
  histogram_tester_.ExpectTotalCount("InputMethod.Manta.Orca.States.Rewrite",
                                     1);
}

TEST_P(EditorStateMetrics, RecordsForNotAllowed) {
  const EditorStates& state = GetParam();
  FakeSystem system;
  FakeContextObserver observer;
  EditorGeolocationMockProvider geolocation_provider(kAllowedCountryCode);
  EditorContext context(&observer, &system, &geolocation_provider);
  EditorMetricsRecorder metrics_recorder(
      &context, EditorOpportunityMode::kNotAllowedForUse);

  metrics_recorder.LogEditorState(state);

  histogram_tester_.ExpectBucketCount(
      "InputMethod.Manta.Orca.States.NotAllowed", state, 1);
  histogram_tester_.ExpectTotalCount("InputMethod.Manta.Orca.States.NotAllowed",
                                     1);
}

TEST_P(EditorStateMetrics, RecordsForInvalidInput) {
  const EditorStates& state = GetParam();
  FakeSystem system;
  FakeContextObserver observer;
  EditorGeolocationMockProvider geolocation_provider(kAllowedCountryCode);
  EditorContext context(&observer, &system, &geolocation_provider);
  EditorMetricsRecorder metrics_recorder(&context,
                                         EditorOpportunityMode::kInvalidInput);

  metrics_recorder.LogEditorState(state);

  histogram_tester_.ExpectBucketCount(
      "InputMethod.Manta.Orca.States.InvalidInput", state, 1);
  histogram_tester_.ExpectTotalCount(
      "InputMethod.Manta.Orca.States.InvalidInput", 1);
}

TEST_P(EditorStateMetrics, RecordsRephraseSegmentForRewrite) {
  const EditorStates& state = GetParam();
  FakeSystem system;
  FakeContextObserver observer;
  EditorGeolocationMockProvider geolocation_provider(kAllowedCountryCode);
  EditorContext context(&observer, &system, &geolocation_provider);
  EditorMetricsRecorder metrics_recorder(&context,
                                         EditorOpportunityMode::kRewrite);

  metrics_recorder.SetTone(EditorTone::kRephrase);
  metrics_recorder.LogEditorState(state);

  histogram_tester_.ExpectBucketCount("InputMethod.Manta.Orca.States.Rephrase",
                                      state, 1);
  histogram_tester_.ExpectTotalCount("InputMethod.Manta.Orca.States.Rephrase",
                                     1);
}

TEST_P(EditorStateMetrics, RecordsEmojifySegmentForRewrite) {
  const EditorStates& state = GetParam();
  FakeSystem system;
  FakeContextObserver observer;
  EditorGeolocationMockProvider geolocation_provider(kAllowedCountryCode);
  EditorContext context(&observer, &system, &geolocation_provider);
  EditorMetricsRecorder metrics_recorder(&context,
                                         EditorOpportunityMode::kRewrite);

  metrics_recorder.SetTone(EditorTone::kEmojify);
  metrics_recorder.LogEditorState(state);

  histogram_tester_.ExpectBucketCount("InputMethod.Manta.Orca.States.Emojify",
                                      state, 1);
  histogram_tester_.ExpectTotalCount("InputMethod.Manta.Orca.States.Emojify",
                                     1);
}

TEST_P(EditorStateMetrics, RecordsShortenSegmentForRewrite) {
  const EditorStates& state = GetParam();
  FakeSystem system;
  FakeContextObserver observer;
  EditorGeolocationMockProvider geolocation_provider(kAllowedCountryCode);
  EditorContext context(&observer, &system, &geolocation_provider);
  EditorMetricsRecorder metrics_recorder(&context,
                                         EditorOpportunityMode::kRewrite);

  metrics_recorder.SetTone(EditorTone::kShorten);
  metrics_recorder.LogEditorState(state);

  histogram_tester_.ExpectBucketCount("InputMethod.Manta.Orca.States.Shorten",
                                      state, 1);
  histogram_tester_.ExpectTotalCount("InputMethod.Manta.Orca.States.Shorten",
                                     1);
}

TEST_P(EditorStateMetrics, RecordsElaborateSegmentForRewrite) {
  const EditorStates& state = GetParam();
  FakeSystem system;
  FakeContextObserver observer;
  EditorGeolocationMockProvider geolocation_provider(kAllowedCountryCode);
  EditorContext context(&observer, &system, &geolocation_provider);
  EditorMetricsRecorder metrics_recorder(&context,
                                         EditorOpportunityMode::kRewrite);

  metrics_recorder.SetTone(EditorTone::kElaborate);
  metrics_recorder.LogEditorState(state);

  histogram_tester_.ExpectBucketCount("InputMethod.Manta.Orca.States.Elaborate",
                                      state, 1);
  histogram_tester_.ExpectTotalCount("InputMethod.Manta.Orca.States.Elaborate",
                                     1);
}

TEST_P(EditorStateMetrics, RecordsFormalizeSegmentForRewrite) {
  const EditorStates& state = GetParam();
  FakeSystem system;
  FakeContextObserver observer;
  EditorGeolocationMockProvider geolocation_provider(kAllowedCountryCode);
  EditorContext context(&observer, &system, &geolocation_provider);
  EditorMetricsRecorder metrics_recorder(&context,
                                         EditorOpportunityMode::kRewrite);

  metrics_recorder.SetTone(EditorTone::kFormalize);
  metrics_recorder.LogEditorState(state);

  histogram_tester_.ExpectBucketCount("InputMethod.Manta.Orca.States.Formalize",
                                      state, 1);
  histogram_tester_.ExpectTotalCount("InputMethod.Manta.Orca.States.Formalize",
                                     1);
}

TEST_P(EditorStateMetrics, RecordsProofreadSegmentForRewrite) {
  const EditorStates& state = GetParam();
  FakeSystem system;
  FakeContextObserver observer;
  EditorGeolocationMockProvider geolocation_provider(kAllowedCountryCode);
  EditorContext context(&observer, &system, &geolocation_provider);
  EditorMetricsRecorder metrics_recorder(&context,
                                         EditorOpportunityMode::kRewrite);

  metrics_recorder.SetTone(EditorTone::kProofread);
  metrics_recorder.LogEditorState(state);

  histogram_tester_.ExpectBucketCount("InputMethod.Manta.Orca.States.Proofread",
                                      state, 1);
  histogram_tester_.ExpectTotalCount("InputMethod.Manta.Orca.States.Proofread",
                                     1);
}

TEST_P(EditorStateMetrics, RecordsFreeformRewriteSegmentForRewrite) {
  const EditorStates& state = GetParam();
  FakeSystem system;
  FakeContextObserver observer;
  EditorGeolocationMockProvider geolocation_provider(kAllowedCountryCode);
  EditorContext context(&observer, &system, &geolocation_provider);
  EditorMetricsRecorder metrics_recorder(&context,
                                         EditorOpportunityMode::kRewrite);

  metrics_recorder.SetTone(EditorTone::kFreeformRewrite);
  metrics_recorder.LogEditorState(state);

  histogram_tester_.ExpectBucketCount(
      "InputMethod.Manta.Orca.States.FreeformRewrite", state, 1);
  histogram_tester_.ExpectTotalCount(
      "InputMethod.Manta.Orca.States.FreeformRewrite", 1);
}

TEST_P(EditorStateMetrics, RecordsUnsetSegmentForRewrite) {
  const EditorStates& state = GetParam();
  FakeSystem system;
  FakeContextObserver observer;
  EditorGeolocationMockProvider geolocation_provider(kAllowedCountryCode);
  EditorContext context(&observer, &system, &geolocation_provider);
  EditorMetricsRecorder metrics_recorder(&context,
                                         EditorOpportunityMode::kRewrite);

  metrics_recorder.SetTone(EditorTone::kUnset);
  metrics_recorder.LogEditorState(state);

  histogram_tester_.ExpectBucketCount("InputMethod.Manta.Orca.States.Unset",
                                      state, 1);
  histogram_tester_.ExpectTotalCount("InputMethod.Manta.Orca.States.Unset", 1);
}

TEST_P(EditorStateMetrics, RecordsUnknownSegmentForRewrite) {
  const EditorStates& state = GetParam();
  FakeSystem system;
  FakeContextObserver observer;
  EditorGeolocationMockProvider geolocation_provider(kAllowedCountryCode);
  EditorContext context(&observer, &system, &geolocation_provider);
  EditorMetricsRecorder metrics_recorder(&context,
                                         EditorOpportunityMode::kRewrite);

  metrics_recorder.SetTone(EditorTone::kUnknown);
  metrics_recorder.LogEditorState(state);

  histogram_tester_.ExpectBucketCount("InputMethod.Manta.Orca.States.Unknown",
                                      state, 1);
  histogram_tester_.ExpectTotalCount("InputMethod.Manta.Orca.States.Unknown",
                                     1);
}

TEST_P(EditorStateMetrics, DoesNotRecordToneSegmentsForWrite) {
  const EditorStates& state = GetParam();
  FakeSystem system;
  FakeContextObserver observer;
  EditorGeolocationMockProvider geolocation_provider(kAllowedCountryCode);
  EditorContext context(&observer, &system, &geolocation_provider);
  EditorMetricsRecorder metrics_recorder(&context,
                                         EditorOpportunityMode::kWrite);

  metrics_recorder.SetTone(EditorTone::kRephrase);
  metrics_recorder.LogEditorState(state);
  metrics_recorder.SetTone(EditorTone::kEmojify);
  metrics_recorder.LogEditorState(state);
  metrics_recorder.SetTone(EditorTone::kShorten);
  metrics_recorder.LogEditorState(state);
  metrics_recorder.SetTone(EditorTone::kElaborate);
  metrics_recorder.LogEditorState(state);
  metrics_recorder.SetTone(EditorTone::kFormalize);
  metrics_recorder.LogEditorState(state);
  metrics_recorder.SetTone(EditorTone::kProofread);
  metrics_recorder.LogEditorState(state);
  metrics_recorder.SetTone(EditorTone::kFreeformRewrite);
  metrics_recorder.LogEditorState(state);
  metrics_recorder.SetTone(EditorTone::kUnset);
  metrics_recorder.LogEditorState(state);
  metrics_recorder.SetTone(EditorTone::kUnknown);
  metrics_recorder.LogEditorState(state);

  histogram_tester_.ExpectTotalCount("InputMethod.Manta.Orca.States.Rephrase",
                                     0);
  histogram_tester_.ExpectTotalCount("InputMethod.Manta.Orca.States.Emojify",
                                     0);
  histogram_tester_.ExpectTotalCount("InputMethod.Manta.Orca.States.Shorten",
                                     0);
  histogram_tester_.ExpectTotalCount("InputMethod.Manta.Orca.States.Elaborate",
                                     0);
  histogram_tester_.ExpectTotalCount("InputMethod.Manta.Orca.States.Formalize",
                                     0);
  histogram_tester_.ExpectTotalCount("InputMethod.Manta.Orca.States.Proofread",
                                     0);
  histogram_tester_.ExpectTotalCount(
      "InputMethod.Manta.Orca.States.FreeformRewrite", 0);
  histogram_tester_.ExpectTotalCount("InputMethod.Manta.Orca.States.Unset", 0);
  histogram_tester_.ExpectTotalCount("InputMethod.Manta.Orca.States.Unknown",
                                     0);
}

TEST_P(EditorStateMetrics, DoesNotRecordToneSegmentsForNotAllowed) {
  const EditorStates& state = GetParam();
  FakeSystem system;
  FakeContextObserver observer;
  EditorGeolocationMockProvider geolocation_provider(kAllowedCountryCode);
  EditorContext context(&observer, &system, &geolocation_provider);
  EditorMetricsRecorder metrics_recorder(
      &context, EditorOpportunityMode::kNotAllowedForUse);

  metrics_recorder.SetTone(EditorTone::kRephrase);
  metrics_recorder.LogEditorState(state);
  metrics_recorder.SetTone(EditorTone::kEmojify);
  metrics_recorder.LogEditorState(state);
  metrics_recorder.SetTone(EditorTone::kShorten);
  metrics_recorder.LogEditorState(state);
  metrics_recorder.SetTone(EditorTone::kElaborate);
  metrics_recorder.LogEditorState(state);
  metrics_recorder.SetTone(EditorTone::kFormalize);
  metrics_recorder.LogEditorState(state);
  metrics_recorder.SetTone(EditorTone::kProofread);
  metrics_recorder.LogEditorState(state);
  metrics_recorder.SetTone(EditorTone::kFreeformRewrite);
  metrics_recorder.LogEditorState(state);
  metrics_recorder.SetTone(EditorTone::kUnset);
  metrics_recorder.LogEditorState(state);
  metrics_recorder.SetTone(EditorTone::kUnknown);
  metrics_recorder.LogEditorState(state);

  histogram_tester_.ExpectTotalCount("InputMethod.Manta.Orca.States.Rephrase",
                                     0);
  histogram_tester_.ExpectTotalCount("InputMethod.Manta.Orca.States.Emojify",
                                     0);
  histogram_tester_.ExpectTotalCount("InputMethod.Manta.Orca.States.Shorten",
                                     0);
  histogram_tester_.ExpectTotalCount("InputMethod.Manta.Orca.States.Elaborate",
                                     0);
  histogram_tester_.ExpectTotalCount("InputMethod.Manta.Orca.States.Formalize",
                                     0);
  histogram_tester_.ExpectTotalCount("InputMethod.Manta.Orca.States.Proofread",
                                     0);
  histogram_tester_.ExpectTotalCount(
      "InputMethod.Manta.Orca.States.FreeformRewrite", 0);
  histogram_tester_.ExpectTotalCount("InputMethod.Manta.Orca.States.Unset", 0);
  histogram_tester_.ExpectTotalCount("InputMethod.Manta.Orca.States.Unknown",
                                     0);
}

TEST_P(EditorStateMetrics, DoesNotRecordToneSegmentsForInvalidInput) {
  const EditorStates& state = GetParam();
  FakeSystem system;
  FakeContextObserver observer;
  EditorGeolocationMockProvider geolocation_provider(kAllowedCountryCode);
  EditorContext context(&observer, &system, &geolocation_provider);
  EditorMetricsRecorder metrics_recorder(&context,
                                         EditorOpportunityMode::kInvalidInput);

  metrics_recorder.SetTone(EditorTone::kRephrase);
  metrics_recorder.LogEditorState(state);
  metrics_recorder.SetTone(EditorTone::kEmojify);
  metrics_recorder.LogEditorState(state);
  metrics_recorder.SetTone(EditorTone::kShorten);
  metrics_recorder.LogEditorState(state);
  metrics_recorder.SetTone(EditorTone::kElaborate);
  metrics_recorder.LogEditorState(state);
  metrics_recorder.SetTone(EditorTone::kFormalize);
  metrics_recorder.LogEditorState(state);
  metrics_recorder.SetTone(EditorTone::kProofread);
  metrics_recorder.LogEditorState(state);
  metrics_recorder.SetTone(EditorTone::kFreeformRewrite);
  metrics_recorder.LogEditorState(state);
  metrics_recorder.SetTone(EditorTone::kUnset);
  metrics_recorder.LogEditorState(state);
  metrics_recorder.SetTone(EditorTone::kUnknown);
  metrics_recorder.LogEditorState(state);

  histogram_tester_.ExpectTotalCount("InputMethod.Manta.Orca.States.Rephrase",
                                     0);
  histogram_tester_.ExpectTotalCount("InputMethod.Manta.Orca.States.Emojify",
                                     0);
  histogram_tester_.ExpectTotalCount("InputMethod.Manta.Orca.States.Shorten",
                                     0);
  histogram_tester_.ExpectTotalCount("InputMethod.Manta.Orca.States.Elaborate",
                                     0);
  histogram_tester_.ExpectTotalCount("InputMethod.Manta.Orca.States.Formalize",
                                     0);
  histogram_tester_.ExpectTotalCount("InputMethod.Manta.Orca.States.Proofread",
                                     0);
  histogram_tester_.ExpectTotalCount(
      "InputMethod.Manta.Orca.States.FreeformRewrite", 0);
  histogram_tester_.ExpectTotalCount("InputMethod.Manta.Orca.States.Unset", 0);
  histogram_tester_.ExpectTotalCount("InputMethod.Manta.Orca.States.Unknown",
                                     0);
}

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
  FakeSystem system;
  FakeContextObserver observer;
  EditorGeolocationMockProvider geolocation_provider(kAllowedCountryCode);
  EditorContext context(&observer, &system, &geolocation_provider);
  EditorMetricsRecorder metrics_recorder(&context, test_case.mode);
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
        {"Proofread", EditorOpportunityMode::kRewrite, EditorTone::kProofread,
         /*number_of_characters=*/1,
         /*tone_string=*/"Proofread"},
        {"FreeformRewrite", EditorOpportunityMode::kRewrite,
         EditorTone::kFreeformRewrite,
         /*number_of_characters=*/1,
         /*tone_string=*/"FreeformRewrite"},
    }),
    [](const testing::TestParamInfo<CharectersInsertedCase> info) {
      return info.param.test_name;
    });

TEST_F(EditorMetricsRecorderTest, WriteCharectersInsertedMetrics) {
  FakeSystem system;
  FakeContextObserver observer;
  EditorGeolocationMockProvider geolocation_provider(kAllowedCountryCode);
  EditorContext context(&observer, &system, &geolocation_provider);
  EditorMetricsRecorder metrics_recorder(&context,
                                         EditorOpportunityMode::kWrite);
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
  FakeSystem system;
  FakeContextObserver observer;
  EditorGeolocationMockProvider geolocation_provider(kAllowedCountryCode);
  EditorContext context(&observer, &system, &geolocation_provider);
  EditorMetricsRecorder metrics_recorder(&context,
                                         EditorOpportunityMode::kRewrite);
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
                             {"Proofread",
                              /*query_tone_string=*/"PROOFREAD",
                              /*freeform_text=*/std::nullopt,
                              /*tone_string=*/"Proofread"},
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

TEST_F(EditorMetricsRecorderTest, WriteServerResponseMetrics) {
  FakeSystem system;
  FakeContextObserver observer;
  EditorGeolocationMockProvider geolocation_provider(kAllowedCountryCode);
  EditorContext context(&observer, &system, &geolocation_provider);
  EditorMetricsRecorder metrics_recorder(&context,
                                         EditorOpportunityMode::kWrite);
  metrics_recorder.SetTone(EditorTone::kUnset);

  metrics_recorder.LogLengthOfLongestResponseFromServer(100);

  histogram_tester_.ExpectUniqueSample(
      base::StrCat({"InputMethod.Manta.Orca.LengthOfLongestResponse.Write"}),
      100, 1);
}

struct ServerResponseRewriteCase {
  std::string test_name;
  EditorTone tone;
  int number_of_characters;
  std::string expected_tone_string;
};

class RewriteServerResponseMetricsTest
    : public EditorMetricsRecorderTest,
      public testing::WithParamInterface<ServerResponseRewriteCase> {};

TEST_P(RewriteServerResponseMetricsTest, RewriteServerResponseMetrics) {
  const ServerResponseRewriteCase& test_case = GetParam();
  FakeSystem system;
  FakeContextObserver observer;
  EditorGeolocationMockProvider geolocation_provider(kAllowedCountryCode);
  EditorContext context(&observer, &system, &geolocation_provider);
  EditorMetricsRecorder metrics_recorder(&context,
                                         EditorOpportunityMode::kRewrite);
  metrics_recorder.SetTone(test_case.tone);

  metrics_recorder.LogLengthOfLongestResponseFromServer(
      test_case.number_of_characters);

  histogram_tester_.ExpectUniqueSample(
      "InputMethod.Manta.Orca.LengthOfLongestResponse.Rewrite",
      test_case.number_of_characters, 1);
  histogram_tester_.ExpectUniqueSample(
      base::StrCat({"InputMethod.Manta.Orca.LengthOfLongestResponse.",
                    test_case.expected_tone_string}),
      test_case.number_of_characters, 1);
}

INSTANTIATE_TEST_SUITE_P(
    RewriteServerResponseMetricsTests,
    RewriteServerResponseMetricsTest,
    testing::ValuesIn<ServerResponseRewriteCase>({
        {"Elaborate", EditorTone::kElaborate,
         /*number_of_characters=*/100,
         /*tone_string=*/"Elaborate"},
        {"Shorten", EditorTone::kShorten,
         /*number_of_characters=*/200,
         /*tone_string=*/"Shorten"},
        {"Formalize", EditorTone::kFormalize,
         /*number_of_characters=*/300,
         /*tone_string=*/"Formalize"},
    }),
    [](const testing::TestParamInfo<ServerResponseRewriteCase> info) {
      return info.param.test_name;
    });

struct LanguageSegmentationCase {
  std::string engine_id;
  std::string expected_histogram_prefix;
};

class EditorStateMetricsSegmentedByLanguage
    : public EditorMetricsRecorderTest,
      public testing::WithParamInterface<LanguageSegmentationCase> {
 public:
  void EnableEnglishFlagsOnly() {
    feature_list_.InitWithFeatures(/*enabled_features=*/{},
                                   /*disabled_features=*/{
                                       features::kOrcaAfrikaans,
                                       features::kOrcaDanish,
                                       features::kOrcaDutch,
                                       features::kOrcaFinnish,
                                       features::kOrcaFrench,
                                       features::kOrcaGerman,
                                       features::kOrcaItalian,
                                       features::kOrcaJapanese,
                                       features::kOrcaNorwegian,
                                       features::kOrcaPolish,
                                       features::kOrcaPortugese,
                                       features::kOrcaSpanish,
                                       features::kOrcaSwedish,
                                   });
  }

 protected:
  ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    EditorMetricsRecorderTest,
    EditorStateMetricsSegmentedByLanguage,
    testing::ValuesIn<LanguageSegmentationCase>({
        // English
        {"xkb:ca:eng:eng", "InputMethod.Manta.Orca.English.States."},
        {"xkb:gb::eng", "InputMethod.Manta.Orca.English.States."},
        {"xkb:gb:extd:eng", "InputMethod.Manta.Orca.English.States."},
        {"xkb:gb:dvorak:eng", "InputMethod.Manta.Orca.English.States."},
        {"xkb:in::eng", "InputMethod.Manta.Orca.English.States."},
        {"xkb:pk::eng", "InputMethod.Manta.Orca.English.States."},
        {"xkb:us:altgr-intl:eng", "InputMethod.Manta.Orca.English.States."},
        {"xkb:us:colemak:eng", "InputMethod.Manta.Orca.English.States."},
        {"xkb:us:dvorak:eng", "InputMethod.Manta.Orca.English.States."},
        {"xkb:us:dvp:eng", "InputMethod.Manta.Orca.English.States."},
        {"xkb:us:intl_pc:eng", "InputMethod.Manta.Orca.English.States."},
        {"xkb:us:intl:eng", "InputMethod.Manta.Orca.English.States."},
        {"xkb:us:workman-intl:eng", "InputMethod.Manta.Orca.English.States."},
        {"xkb:us:workman:eng", "InputMethod.Manta.Orca.English.States."},
        {"xkb:us::eng", "InputMethod.Manta.Orca.English.States."},
        {"xkb:za:gb:eng", "InputMethod.Manta.Orca.English.States."},
        // French
        {"xkb:be::fra", "InputMethod.Manta.Orca.French.States."},
        {"xkb:ca::fra", "InputMethod.Manta.Orca.French.States."},
        {"xkb:ca:multix:fra", "InputMethod.Manta.Orca.French.States."},
        {"xkb:fr::fra", "InputMethod.Manta.Orca.French.States."},
        {"xkb:fr:bepo:fra", "InputMethod.Manta.Orca.French.States."},
        {"xkb:ch:fr:fra", "InputMethod.Manta.Orca.French.States."},
        // German
        {"xkb:be::ger", "InputMethod.Manta.Orca.German.States."},
        {"xkb:de::ger", "InputMethod.Manta.Orca.German.States."},
        {"xkb:de:neo:ger", "InputMethod.Manta.Orca.German.States."},
        {"xkb:ch::ger", "InputMethod.Manta.Orca.German.States."},
        // Japanese
        {"xkb:jp::jpn", "InputMethod.Manta.Orca.Japanese.States."},
        {"nacl_mozc_us", "InputMethod.Manta.Orca.Japanese.States."},
        {"nacl_mozc_jp", "InputMethod.Manta.Orca.Japanese.States."},
        // Danish
        {"xkb:dk::dan", "InputMethod.Manta.Orca.Danish.States."},
        // Dutch
        {"xkb:be::nld", "InputMethod.Manta.Orca.Dutch.States."},
        {"xkb:us:intl_pc:nld", "InputMethod.Manta.Orca.Dutch.States."},
        {"xkb:us:intl:nld", "InputMethod.Manta.Orca.Dutch.States."},
        // Finnish
        {"xkb:fi::fin", "InputMethod.Manta.Orca.Finnish.States."},
        // Italian
        {"xkb:it::ita", "InputMethod.Manta.Orca.Italian.States."},
        // Norwegian
        {"xkb:no::nob", "InputMethod.Manta.Orca.Norwegian.States."},
        // Polish
        {"xkb:pl::pol", "InputMethod.Manta.Orca.Polish.States."},
        // Portugese
        {"xkb:br::por", "InputMethod.Manta.Orca.Portugese.States."},
        {"xkb:pt::por", "InputMethod.Manta.Orca.Portugese.States."},
        {"xkb:us:intl_pc:por", "InputMethod.Manta.Orca.Portugese.States."},
        {"xkb:us:intl:por", "InputMethod.Manta.Orca.Portugese.States."},
        // Spanish
        {"xkb:latam::spa", "InputMethod.Manta.Orca.Spanish.States."},
        {"xkb:es::spa", "InputMethod.Manta.Orca.Spanish.States."},
        // Swedish
        {"xkb:se::swe", "InputMethod.Manta.Orca.Swedish.States."},
    }));

TEST_P(EditorStateMetricsSegmentedByLanguage,
       WritesToCorrectHistogramForWriteMode) {
  const LanguageSegmentationCase& test_case = GetParam();
  const std::string expected_histogram =
      base::StrCat({test_case.expected_histogram_prefix, "Write"});
  FakeSystem system;
  FakeContextObserver observer;
  EditorGeolocationMockProvider geolocation_provider(kAllowedCountryCode);
  EditorContext context(&observer, &system, &geolocation_provider);
  EditorMetricsRecorder recorder(&context, EditorOpportunityMode::kWrite);

  context.OnActivateIme(test_case.engine_id);
  recorder.LogEditorState(EditorStates::kNativeUIShown);
  recorder.LogEditorState(EditorStates::kNativeRequest);
  recorder.LogEditorState(EditorStates::kDismiss);

  histogram_tester_.ExpectBucketCount(expected_histogram,
                                      EditorStates::kNativeUIShown, 1);
  histogram_tester_.ExpectBucketCount(expected_histogram,
                                      EditorStates::kNativeRequest, 1);
  histogram_tester_.ExpectBucketCount(expected_histogram,
                                      EditorStates::kDismiss, 1);
  histogram_tester_.ExpectTotalCount(expected_histogram, 3);
}

TEST_P(EditorStateMetricsSegmentedByLanguage,
       WritesToCorrectHistogramForRewriteMode) {
  const LanguageSegmentationCase& test_case = GetParam();
  const std::string expected_histogram =
      base::StrCat({test_case.expected_histogram_prefix, "Rewrite"});
  FakeSystem system;
  FakeContextObserver observer;
  EditorGeolocationMockProvider geolocation_provider(kAllowedCountryCode);
  EditorContext context(&observer, &system, &geolocation_provider);
  EditorMetricsRecorder recorder(&context, EditorOpportunityMode::kRewrite);

  context.OnActivateIme(test_case.engine_id);
  recorder.LogEditorState(EditorStates::kNativeUIShown);
  recorder.LogEditorState(EditorStates::kNativeRequest);
  recorder.LogEditorState(EditorStates::kDismiss);

  histogram_tester_.ExpectBucketCount(expected_histogram,
                                      EditorStates::kNativeUIShown, 1);
  histogram_tester_.ExpectBucketCount(expected_histogram,
                                      EditorStates::kNativeRequest, 1);
  histogram_tester_.ExpectBucketCount(expected_histogram,
                                      EditorStates::kDismiss, 1);
  histogram_tester_.ExpectTotalCount(expected_histogram, 3);
}

TEST_P(EditorStateMetricsSegmentedByLanguage, DoesntRecordIfFlagDisabled) {
  const LanguageSegmentationCase& test_case = GetParam();
  const std::string expected_histogram =
      base::StrCat({test_case.expected_histogram_prefix, "Rewrite"});
  EnableEnglishFlagsOnly();
  FakeSystem system;
  FakeContextObserver observer;
  EditorGeolocationMockProvider geolocation_provider(kAllowedCountryCode);
  EditorContext context(&observer, &system, &geolocation_provider);
  EditorMetricsRecorder recorder(&context, EditorOpportunityMode::kRewrite);

  context.OnActivateIme(test_case.engine_id);
  recorder.LogEditorState(EditorStates::kNativeUIShown);
  recorder.LogEditorState(EditorStates::kNativeRequest);
  recorder.LogEditorState(EditorStates::kDismiss);

  histogram_tester_.ExpectTotalCount(expected_histogram, 0);
}

class EditorAuxiliaryMetricsSegmentedByEnglishAndOther
    : public EditorMetricsRecorderTest,
      public testing::WithParamInterface<LanguageSegmentationCase> {
 public:
  void EnableEnglishFlagsOnly() {
    feature_list_.InitWithFeatures(/*enabled_features=*/{},
                                   /*disabled_features=*/{
                                       features::kOrcaAfrikaans,
                                       features::kOrcaDanish,
                                       features::kOrcaDutch,
                                       features::kOrcaFinnish,
                                       features::kOrcaFrench,
                                       features::kOrcaGerman,
                                       features::kOrcaItalian,
                                       features::kOrcaJapanese,
                                       features::kOrcaNorwegian,
                                       features::kOrcaPolish,
                                       features::kOrcaPortugese,
                                       features::kOrcaSpanish,
                                       features::kOrcaSwedish,
                                   });
  }

 protected:
  ScopedFeatureList feature_list_;
};

// Note that auxiliary metrics are not segmented by each language enabled, but
// by two high level buckets; kEnglish and kOther. This is to prevent an
// explosion of new metrics recorded, whilst still providing auxiliary info for
// the English segment of users.
INSTANTIATE_TEST_SUITE_P(
    EditorMetricsRecorderTest,
    EditorAuxiliaryMetricsSegmentedByEnglishAndOther,
    testing::ValuesIn<LanguageSegmentationCase>({
        // English
        {"xkb:ca:eng:eng", "InputMethod.Manta.Orca.English."},
        {"xkb:gb::eng", "InputMethod.Manta.Orca.English."},
        {"xkb:gb:extd:eng", "InputMethod.Manta.Orca.English."},
        {"xkb:gb:dvorak:eng", "InputMethod.Manta.Orca.English."},
        {"xkb:in::eng", "InputMethod.Manta.Orca.English."},
        {"xkb:pk::eng", "InputMethod.Manta.Orca.English."},
        {"xkb:us:altgr-intl:eng", "InputMethod.Manta.Orca.English."},
        {"xkb:us:colemak:eng", "InputMethod.Manta.Orca.English."},
        {"xkb:us:dvorak:eng", "InputMethod.Manta.Orca.English."},
        {"xkb:us:dvp:eng", "InputMethod.Manta.Orca.English."},
        {"xkb:us:intl_pc:eng", "InputMethod.Manta.Orca.English."},
        {"xkb:us:intl:eng", "InputMethod.Manta.Orca.English."},
        {"xkb:us:workman-intl:eng", "InputMethod.Manta.Orca.English."},
        {"xkb:us:workman:eng", "InputMethod.Manta.Orca.English."},
        {"xkb:us::eng", "InputMethod.Manta.Orca.English."},
        {"xkb:za:gb:eng", "InputMethod.Manta.Orca.English."},
        // French
        {"xkb:be::fra", "InputMethod.Manta.Orca.Other."},
        {"xkb:ca::fra", "InputMethod.Manta.Orca.Other."},
        {"xkb:ca:multix:fra", "InputMethod.Manta.Orca.Other."},
        {"xkb:fr::fra", "InputMethod.Manta.Orca.Other."},
        {"xkb:fr:bepo:fra", "InputMethod.Manta.Orca.Other."},
        {"xkb:ch:fr:fra", "InputMethod.Manta.Orca.Other."},
        // German
        {"xkb:be::ger", "InputMethod.Manta.Orca.Other."},
        {"xkb:de::ger", "InputMethod.Manta.Orca.Other."},
        {"xkb:de:neo:ger", "InputMethod.Manta.Orca.Other."},
        {"xkb:ch::ger", "InputMethod.Manta.Orca.Other."},
        // Japanese
        {"xkb:jp::jpn", "InputMethod.Manta.Orca.Other."},
        {"nacl_mozc_us", "InputMethod.Manta.Orca.Other."},
        {"nacl_mozc_jp", "InputMethod.Manta.Orca.Other."},
        // Danish
        {"xkb:dk::dan", "InputMethod.Manta.Orca.Other."},
        // Dutch
        {"xkb:be::nld", "InputMethod.Manta.Orca.Other."},
        {"xkb:us:intl_pc:nld", "InputMethod.Manta.Orca.Other."},
        {"xkb:us:intl:nld", "InputMethod.Manta.Orca.Other."},
        // Finnish
        {"xkb:fi::fin", "InputMethod.Manta.Orca.Other."},
        // Italian
        {"xkb:it::ita", "InputMethod.Manta.Orca.Other."},
        // Norwegian
        {"xkb:no::nob", "InputMethod.Manta.Orca.Other."},
        // Polish
        {"xkb:pl::pol", "InputMethod.Manta.Orca.Other."},
        // Portugese
        {"xkb:br::por", "InputMethod.Manta.Orca.Other."},
        {"xkb:pt::por", "InputMethod.Manta.Orca.Other."},
        {"xkb:us:intl_pc:por", "InputMethod.Manta.Orca.Other."},
        {"xkb:us:intl:por", "InputMethod.Manta.Orca.Other."},
        // Spanish
        {"xkb:latam::spa", "InputMethod.Manta.Orca.Other."},
        {"xkb:es::spa", "InputMethod.Manta.Orca.Other."},
        // Swedish
        {"xkb:se::swe", "InputMethod.Manta.Orca.Other."},
    }));

TEST_P(EditorAuxiliaryMetricsSegmentedByEnglishAndOther,
       NumberOfCharactersInsertedForWrite) {
  const LanguageSegmentationCase& test_case = GetParam();
  const std::string expected_histogram = base::StrCat(
      {test_case.expected_histogram_prefix, "CharactersInserted.Write"});
  FakeSystem system;
  FakeContextObserver observer;
  EditorGeolocationMockProvider geolocation_provider(kAllowedCountryCode);
  EditorContext context(&observer, &system, &geolocation_provider);
  EditorMetricsRecorder recorder(&context, EditorOpportunityMode::kWrite);

  context.OnActivateIme(test_case.engine_id);
  recorder.LogNumberOfCharactersInserted(100);

  histogram_tester_.ExpectBucketCount(expected_histogram, 100, 1);
  histogram_tester_.ExpectTotalCount(expected_histogram, 1);
}

TEST_P(EditorAuxiliaryMetricsSegmentedByEnglishAndOther,
       NumberOfCharactersInsertedForRewrite) {
  const LanguageSegmentationCase& test_case = GetParam();
  const std::string expected_histogram = base::StrCat(
      {test_case.expected_histogram_prefix, "CharactersInserted.Rewrite"});
  FakeSystem system;
  FakeContextObserver observer;
  EditorGeolocationMockProvider geolocation_provider(kAllowedCountryCode);
  EditorContext context(&observer, &system, &geolocation_provider);
  EditorMetricsRecorder recorder(&context, EditorOpportunityMode::kRewrite);

  context.OnActivateIme(test_case.engine_id);
  recorder.LogNumberOfCharactersInserted(100);

  histogram_tester_.ExpectBucketCount(expected_histogram, 100, 1);
  histogram_tester_.ExpectTotalCount(expected_histogram, 1);
}

TEST_P(EditorAuxiliaryMetricsSegmentedByEnglishAndOther,
       NumberOfCharactersInsertedWithFlagDisabled) {
  const LanguageSegmentationCase& test_case = GetParam();
  const std::string expected_histogram = base::StrCat(
      {test_case.expected_histogram_prefix, "CharactersInserted.Rewrite"});
  EnableEnglishFlagsOnly();
  FakeSystem system;
  FakeContextObserver observer;
  EditorGeolocationMockProvider geolocation_provider(kAllowedCountryCode);
  EditorContext context(&observer, &system, &geolocation_provider);
  EditorMetricsRecorder recorder(&context, EditorOpportunityMode::kRewrite);

  context.OnActivateIme(test_case.engine_id);
  recorder.LogNumberOfCharactersInserted(100);

  histogram_tester_.ExpectTotalCount(expected_histogram, 0);
}

TEST_P(EditorAuxiliaryMetricsSegmentedByEnglishAndOther,
       CharactersSelectedForInsertWithWrite) {
  const LanguageSegmentationCase& test_case = GetParam();
  const std::string expected_histogram =
      base::StrCat({test_case.expected_histogram_prefix,
                    "CharactersSelectedForInsert.Write"});
  FakeSystem system;
  FakeContextObserver observer;
  EditorGeolocationMockProvider geolocation_provider(kAllowedCountryCode);
  EditorContext context(&observer, &system, &geolocation_provider);
  EditorMetricsRecorder recorder(&context, EditorOpportunityMode::kWrite);

  context.OnActivateIme(test_case.engine_id);
  recorder.LogNumberOfCharactersSelectedForInsert(100);

  histogram_tester_.ExpectBucketCount(expected_histogram, 100, 1);
  histogram_tester_.ExpectTotalCount(expected_histogram, 1);
}

TEST_P(EditorAuxiliaryMetricsSegmentedByEnglishAndOther,
       CharactersSelectedForInsertWithRewrite) {
  const LanguageSegmentationCase& test_case = GetParam();
  const std::string expected_histogram =
      base::StrCat({test_case.expected_histogram_prefix,
                    "CharactersSelectedForInsert.Rewrite"});
  FakeSystem system;
  FakeContextObserver observer;
  EditorGeolocationMockProvider geolocation_provider(kAllowedCountryCode);
  EditorContext context(&observer, &system, &geolocation_provider);
  EditorMetricsRecorder recorder(&context, EditorOpportunityMode::kRewrite);

  context.OnActivateIme(test_case.engine_id);
  recorder.LogNumberOfCharactersSelectedForInsert(100);

  histogram_tester_.ExpectBucketCount(expected_histogram, 100, 1);
  histogram_tester_.ExpectTotalCount(expected_histogram, 1);
}

TEST_P(EditorAuxiliaryMetricsSegmentedByEnglishAndOther,
       CharactersSelectedForInsertWithFlagDisabled) {
  const LanguageSegmentationCase& test_case = GetParam();
  const std::string expected_histogram =
      base::StrCat({test_case.expected_histogram_prefix,
                    "CharactersSelectedForInsert.Rewrite"});
  EnableEnglishFlagsOnly();
  FakeSystem system;
  FakeContextObserver observer;
  EditorGeolocationMockProvider geolocation_provider(kAllowedCountryCode);
  EditorContext context(&observer, &system, &geolocation_provider);
  EditorMetricsRecorder recorder(&context, EditorOpportunityMode::kRewrite);

  context.OnActivateIme(test_case.engine_id);
  recorder.LogNumberOfCharactersSelectedForInsert(100);

  histogram_tester_.ExpectTotalCount(expected_histogram, 0);
}

TEST_P(EditorAuxiliaryMetricsSegmentedByEnglishAndOther,
       NumberOfResponsesFromServerWithWrite) {
  const LanguageSegmentationCase& test_case = GetParam();
  const std::string expected_histogram =
      base::StrCat({test_case.expected_histogram_prefix, "NumResponses.Write"});
  FakeSystem system;
  FakeContextObserver observer;
  EditorGeolocationMockProvider geolocation_provider(kAllowedCountryCode);
  EditorContext context(&observer, &system, &geolocation_provider);
  EditorMetricsRecorder recorder(&context, EditorOpportunityMode::kWrite);

  context.OnActivateIme(test_case.engine_id);
  recorder.LogNumberOfResponsesFromServer(3);

  histogram_tester_.ExpectBucketCount(expected_histogram, 3, 1);
  histogram_tester_.ExpectTotalCount(expected_histogram, 1);
}

TEST_P(EditorAuxiliaryMetricsSegmentedByEnglishAndOther,
       NumberOfResponsesFromServerWithRewrite) {
  const LanguageSegmentationCase& test_case = GetParam();
  const std::string expected_histogram = base::StrCat(
      {test_case.expected_histogram_prefix, "NumResponses.Rewrite"});
  FakeSystem system;
  FakeContextObserver observer;
  EditorGeolocationMockProvider geolocation_provider(kAllowedCountryCode);
  EditorContext context(&observer, &system, &geolocation_provider);
  EditorMetricsRecorder recorder(&context, EditorOpportunityMode::kRewrite);

  context.OnActivateIme(test_case.engine_id);
  recorder.LogNumberOfResponsesFromServer(3);

  histogram_tester_.ExpectBucketCount(expected_histogram, 3, 1);
  histogram_tester_.ExpectTotalCount(expected_histogram, 1);
}

TEST_P(EditorAuxiliaryMetricsSegmentedByEnglishAndOther,
       NumberOfResponsesFromServerWithFlagDisabled) {
  const LanguageSegmentationCase& test_case = GetParam();
  const std::string expected_histogram = base::StrCat(
      {test_case.expected_histogram_prefix, "NumResponses.Rewrite"});
  EnableEnglishFlagsOnly();
  FakeSystem system;
  FakeContextObserver observer;
  EditorGeolocationMockProvider geolocation_provider(kAllowedCountryCode);
  EditorContext context(&observer, &system, &geolocation_provider);
  EditorMetricsRecorder recorder(&context, EditorOpportunityMode::kRewrite);

  context.OnActivateIme(test_case.engine_id);
  recorder.LogNumberOfResponsesFromServer(3);

  histogram_tester_.ExpectTotalCount(expected_histogram, 0);
}

TEST_P(EditorAuxiliaryMetricsSegmentedByEnglishAndOther,
       LengthOfLongestResponseWithWrite) {
  const LanguageSegmentationCase& test_case = GetParam();
  const std::string expected_histogram = base::StrCat(
      {test_case.expected_histogram_prefix, "LengthOfLongestResponse.Write"});
  FakeSystem system;
  FakeContextObserver observer;
  EditorGeolocationMockProvider geolocation_provider(kAllowedCountryCode);
  EditorContext context(&observer, &system, &geolocation_provider);
  EditorMetricsRecorder recorder(&context, EditorOpportunityMode::kWrite);

  context.OnActivateIme(test_case.engine_id);
  recorder.LogLengthOfLongestResponseFromServer(2000);

  histogram_tester_.ExpectBucketCount(expected_histogram, 2000, 1);
  histogram_tester_.ExpectTotalCount(expected_histogram, 1);
}

TEST_P(EditorAuxiliaryMetricsSegmentedByEnglishAndOther,
       LengthOfLongestResponseWithRewrite) {
  const LanguageSegmentationCase& test_case = GetParam();
  const std::string expected_histogram = base::StrCat(
      {test_case.expected_histogram_prefix, "LengthOfLongestResponse.Rewrite"});
  FakeSystem system;
  FakeContextObserver observer;
  EditorGeolocationMockProvider geolocation_provider(kAllowedCountryCode);
  EditorContext context(&observer, &system, &geolocation_provider);
  EditorMetricsRecorder recorder(&context, EditorOpportunityMode::kRewrite);

  context.OnActivateIme(test_case.engine_id);
  recorder.LogLengthOfLongestResponseFromServer(2000);

  histogram_tester_.ExpectBucketCount(expected_histogram, 2000, 1);
  histogram_tester_.ExpectTotalCount(expected_histogram, 1);
}

TEST_P(EditorAuxiliaryMetricsSegmentedByEnglishAndOther,
       LengthOfLongestResponseWithFlagDisabled) {
  const LanguageSegmentationCase& test_case = GetParam();
  const std::string expected_histogram = base::StrCat(
      {test_case.expected_histogram_prefix, "LengthOfLongestResponse.Rewrite"});
  EnableEnglishFlagsOnly();
  FakeSystem system;
  FakeContextObserver observer;
  EditorGeolocationMockProvider geolocation_provider(kAllowedCountryCode);
  EditorContext context(&observer, &system, &geolocation_provider);
  EditorMetricsRecorder recorder(&context, EditorOpportunityMode::kRewrite);

  context.OnActivateIme(test_case.engine_id);
  recorder.LogLengthOfLongestResponseFromServer(2000);

  histogram_tester_.ExpectTotalCount(expected_histogram, 0);
}

struct CriticalStateCase {
  EditorStates editor_state;
  EditorCriticalStates expected_critical_state;
};

class WritesCriticalStateMetrics
    : public EditorMetricsRecorderTest,
      public testing::WithParamInterface<CriticalStateCase> {};

INSTANTIATE_TEST_SUITE_P(
    EditorMetricsRecorderTest,
    WritesCriticalStateMetrics,
    testing::ValuesIn<CriticalStateCase>({
        {EditorStates::kNativeUIShown, EditorCriticalStates::kShowUI},
        {EditorStates::kPromoCardImpression, EditorCriticalStates::kShowUI},
        {EditorStates::kNativeRequest, EditorCriticalStates::kRequestTriggered},
        {EditorStates::kWebUIRequest, EditorCriticalStates::kRequestTriggered},
        {EditorStates::kInsert, EditorCriticalStates::kTextInserted},
    }));

TEST_P(WritesCriticalStateMetrics, ForRewrite) {
  const auto& [editor_state, expected_critical_state] = GetParam();
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  ukm::SourceId source_id = ukm_recorder.GetNewSourceID();
  ukm_recorder.UpdateSourceURL(source_id, GURL("https://test.example.com"));
  FakeSystem system(source_id);
  FakeContextObserver observer;
  EditorGeolocationMockProvider geolocation_provider(kAllowedCountryCode);
  EditorContext context(&observer, &system, &geolocation_provider);
  EditorMetricsRecorder recorder(&context, EditorOpportunityMode::kRewrite);

  recorder.LogEditorState(editor_state);

  auto entries = ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  ASSERT_EQ(entries.size(), 1u);
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entries[0], UkmEntry::kEditorCriticalStatesName,
      static_cast<int>(expected_critical_state));
}

TEST_P(WritesCriticalStateMetrics, ForWrite) {
  const auto& [editor_state, expected_critical_state] = GetParam();
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  ukm::SourceId source_id = ukm_recorder.GetNewSourceID();
  ukm_recorder.UpdateSourceURL(source_id, GURL("https://test.example.com"));
  FakeSystem system(source_id);
  FakeContextObserver observer;
  EditorGeolocationMockProvider geolocation_provider(kAllowedCountryCode);
  EditorContext context(&observer, &system, &geolocation_provider);
  EditorMetricsRecorder recorder(&context, EditorOpportunityMode::kWrite);

  recorder.LogEditorState(editor_state);

  auto entries = ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  ASSERT_EQ(entries.size(), 1u);
  ukm::TestAutoSetUkmRecorder::ExpectEntryMetric(
      entries[0], UkmEntry::kEditorCriticalStatesName,
      static_cast<int>(expected_critical_state));
}

class DoesNotWriteCriticalStateMetrics
    : public EditorMetricsRecorderTest,
      public testing::WithParamInterface<EditorStates> {};

INSTANTIATE_TEST_SUITE_P(EditorMetricsRecorderTest,
                         DoesNotWriteCriticalStateMetrics,
                         testing::ValuesIn<EditorStates>({
                             EditorStates::kNativeUIShowOpportunity,
                             EditorStates::kDismiss,
                             EditorStates::kRefineRequest,
                             EditorStates::kSuccessResponse,
                             EditorStates::kErrorResponse,
                             EditorStates::kThumbsUp,
                             EditorStates::kThumbsDown,
                             EditorStates::kReturnToPreviousSuggestions,
                             EditorStates::kClickCloseButton,
                             EditorStates::kApproveConsent,
                             EditorStates::kDeclineConsent,
                             EditorStates::kBlocked,
                             EditorStates::kBlockedByUnsupportedRegion,
                             EditorStates::kBlockedByConsent,
                             EditorStates::kBlockedBySetting,
                             EditorStates::kBlockedByTextLength,
                             EditorStates::kBlockedByUrl,
                             EditorStates::kBlockedByApp,
                             EditorStates::kBlockedByInputMethod,
                             EditorStates::kBlockedByInputType,
                             EditorStates::kBlockedByAppType,
                             EditorStates::kBlockedByInvalidFormFactor,
                             EditorStates::kBlockedByNetworkStatus,
                             EditorStates::kErrorUnknown,
                             EditorStates::kErrorInvalidArgument,
                             EditorStates::kErrorResourceExhausted,
                             EditorStates::kErrorBackendFailure,
                             EditorStates::kErrorNoInternetConnection,
                             EditorStates::kErrorUnsupportedLanguage,
                             EditorStates::kErrorBlockedOutputs,
                             EditorStates::kErrorRestrictedRegion,
                             EditorStates::kPromoCardExplicitDismissal,
                             EditorStates::kConsentScreenImpression,
                             EditorStates::kTextInsertionRequested,
                             EditorStates::kTextQueuedForInsertion,
                             EditorStates::kRequest,
                             EditorStates::kBlockedByUnsupportedCapability,
                             EditorStates::kBlockedByUnknownCapability,
                             EditorStates::kBlockedByPolicy,
                         }));

TEST_P(DoesNotWriteCriticalStateMetrics, ForRewrite) {
  const EditorStates& editor_state = GetParam();
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  ukm::SourceId source_id = ukm_recorder.GetNewSourceID();
  ukm_recorder.UpdateSourceURL(source_id, GURL("https://test.example.com"));
  FakeSystem system(source_id);
  FakeContextObserver observer;
  EditorGeolocationMockProvider geolocation_provider(kAllowedCountryCode);
  EditorContext context(&observer, &system, &geolocation_provider);
  EditorMetricsRecorder recorder(&context, EditorOpportunityMode::kRewrite);

  recorder.LogEditorState(editor_state);

  auto entries = ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_THAT(entries, IsEmpty());
}

TEST_P(DoesNotWriteCriticalStateMetrics, ForWrite) {
  const EditorStates& editor_state = GetParam();
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  ukm::SourceId source_id = ukm_recorder.GetNewSourceID();
  ukm_recorder.UpdateSourceURL(source_id, GURL("https://test.example.com"));
  FakeSystem system(source_id);
  FakeContextObserver observer;
  EditorGeolocationMockProvider geolocation_provider(kAllowedCountryCode);
  EditorContext context(&observer, &system, &geolocation_provider);
  EditorMetricsRecorder recorder(&context, EditorOpportunityMode::kWrite);

  recorder.LogEditorState(editor_state);

  auto entries = ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_THAT(entries, IsEmpty());
}

}  // namespace
}  // namespace ash::input_method
