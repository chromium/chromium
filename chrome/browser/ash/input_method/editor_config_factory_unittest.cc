// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/editor_config_factory.h"

#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::input_method {
namespace {

using base::test::ScopedFeatureList;
using orca::mojom::PresetTextQueryType;
using testing::UnorderedElementsAre;

TEST(EditorConfigFactoryTest, BuildsCorrectlyForEnglish) {
  orca::mojom::EditorConfigPtr config =
      BuildConfigFor(LanguageCategory::kEnglish);

  EXPECT_THAT(
      config->allowed_query_types,
      UnorderedElementsAre(orca::mojom::PresetTextQueryType::kShorten,
                           orca::mojom::PresetTextQueryType::kElaborate,
                           orca::mojom::PresetTextQueryType::kRephrase,
                           orca::mojom::PresetTextQueryType::kFormalize,
                           orca::mojom::PresetTextQueryType::kEmojify,
                           orca::mojom::PresetTextQueryType::kProofread));
}

TEST(EditorConfigFactoryTest, EnglishWithShortenDisabled) {
  ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({}, {features::kOrcaShorten});

  orca::mojom::EditorConfigPtr config =
      BuildConfigFor(LanguageCategory::kEnglish);

  EXPECT_THAT(
      config->allowed_query_types,
      UnorderedElementsAre(orca::mojom::PresetTextQueryType::kElaborate,
                           orca::mojom::PresetTextQueryType::kRephrase,
                           orca::mojom::PresetTextQueryType::kFormalize,
                           orca::mojom::PresetTextQueryType::kEmojify,
                           orca::mojom::PresetTextQueryType::kProofread));
}

TEST(EditorConfigFactoryTest, EnglishWithElaborateDisabled) {
  ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({}, {features::kOrcaElaborate});

  orca::mojom::EditorConfigPtr config =
      BuildConfigFor(LanguageCategory::kEnglish);

  EXPECT_THAT(
      config->allowed_query_types,
      UnorderedElementsAre(orca::mojom::PresetTextQueryType::kShorten,
                           orca::mojom::PresetTextQueryType::kRephrase,
                           orca::mojom::PresetTextQueryType::kFormalize,
                           orca::mojom::PresetTextQueryType::kEmojify,
                           orca::mojom::PresetTextQueryType::kProofread));
}

TEST(EditorConfigFactoryTest, EnglishWithRephraseDisabled) {
  ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({}, {features::kOrcaRephrase});

  orca::mojom::EditorConfigPtr config =
      BuildConfigFor(LanguageCategory::kEnglish);

  EXPECT_THAT(
      config->allowed_query_types,
      UnorderedElementsAre(orca::mojom::PresetTextQueryType::kShorten,
                           orca::mojom::PresetTextQueryType::kElaborate,
                           orca::mojom::PresetTextQueryType::kFormalize,
                           orca::mojom::PresetTextQueryType::kEmojify,
                           orca::mojom::PresetTextQueryType::kProofread));
}

TEST(EditorConfigFactoryTest, EnglishWithFormalizeDisabled) {
  ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({}, {features::kOrcaFormalize});

  orca::mojom::EditorConfigPtr config =
      BuildConfigFor(LanguageCategory::kEnglish);

  EXPECT_THAT(
      config->allowed_query_types,
      UnorderedElementsAre(orca::mojom::PresetTextQueryType::kShorten,
                           orca::mojom::PresetTextQueryType::kElaborate,
                           orca::mojom::PresetTextQueryType::kRephrase,
                           orca::mojom::PresetTextQueryType::kEmojify,
                           orca::mojom::PresetTextQueryType::kProofread));
}

TEST(EditorConfigFactoryTest, EnglishWithEmojifyDisabled) {
  ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({}, {features::kOrcaEmojify});

  orca::mojom::EditorConfigPtr config =
      BuildConfigFor(LanguageCategory::kEnglish);

  EXPECT_THAT(
      config->allowed_query_types,
      UnorderedElementsAre(orca::mojom::PresetTextQueryType::kShorten,
                           orca::mojom::PresetTextQueryType::kElaborate,
                           orca::mojom::PresetTextQueryType::kRephrase,
                           orca::mojom::PresetTextQueryType::kFormalize,
                           orca::mojom::PresetTextQueryType::kProofread));
}

TEST(EditorConfigFactoryTest, EnglishWithProofreadDisabled) {
  ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({}, {features::kOrcaProofread});

  orca::mojom::EditorConfigPtr config =
      BuildConfigFor(LanguageCategory::kEnglish);

  EXPECT_THAT(config->allowed_query_types,
              UnorderedElementsAre(orca::mojom::PresetTextQueryType::kShorten,
                                   orca::mojom::PresetTextQueryType::kElaborate,
                                   orca::mojom::PresetTextQueryType::kRephrase,
                                   orca::mojom::PresetTextQueryType::kFormalize,
                                   orca::mojom::PresetTextQueryType::kEmojify));
}

TEST(EditorConfigFactoryTest, BuildsCorrectlyForFrench) {
  orca::mojom::EditorConfigPtr config =
      BuildConfigFor(LanguageCategory::kFrench);

  EXPECT_THAT(
      config->allowed_query_types,
      UnorderedElementsAre(orca::mojom::PresetTextQueryType::kShorten,
                           orca::mojom::PresetTextQueryType::kElaborate,
                           orca::mojom::PresetTextQueryType::kRephrase,
                           orca::mojom::PresetTextQueryType::kFormalize,
                           orca::mojom::PresetTextQueryType::kEmojify,
                           orca::mojom::PresetTextQueryType::kProofread));
}

class InternationalizedCases
    : public testing::Test,
      public testing::WithParamInterface<LanguageCategory> {};

INSTANTIATE_TEST_SUITE_P(EditorConfigFactoryTest,
                         InternationalizedCases,
                         testing::ValuesIn<LanguageCategory>({
                             LanguageCategory::kDanish,
                             LanguageCategory::kDutch,
                             LanguageCategory::kFinnish,
                             LanguageCategory::kFrench,
                             LanguageCategory::kGerman,
                             LanguageCategory::kItalian,
                             LanguageCategory::kJapanese,
                             LanguageCategory::kNorwegian,
                             LanguageCategory::kPortugese,
                             LanguageCategory::kSpanish,
                             LanguageCategory::kSwedish,
                         }));

TEST_P(InternationalizedCases, WithNothingDisabled) {
  const LanguageCategory& language_category = GetParam();

  orca::mojom::EditorConfigPtr config = BuildConfigFor(language_category);

  EXPECT_THAT(
      config->allowed_query_types,
      UnorderedElementsAre(orca::mojom::PresetTextQueryType::kShorten,
                           orca::mojom::PresetTextQueryType::kElaborate,
                           orca::mojom::PresetTextQueryType::kRephrase,
                           orca::mojom::PresetTextQueryType::kFormalize,
                           orca::mojom::PresetTextQueryType::kEmojify,
                           orca::mojom::PresetTextQueryType::kProofread));
}

TEST_P(InternationalizedCases, WithShortenDisabled) {
  const LanguageCategory& language_category = GetParam();
  ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({}, {features::kOrcaInternationalizeShorten});

  orca::mojom::EditorConfigPtr config = BuildConfigFor(language_category);

  EXPECT_THAT(
      config->allowed_query_types,
      UnorderedElementsAre(orca::mojom::PresetTextQueryType::kElaborate,
                           orca::mojom::PresetTextQueryType::kRephrase,
                           orca::mojom::PresetTextQueryType::kFormalize,
                           orca::mojom::PresetTextQueryType::kEmojify,
                           orca::mojom::PresetTextQueryType::kProofread));
}

TEST_P(InternationalizedCases, WithElaborateDisabled) {
  const LanguageCategory& language_category = GetParam();
  ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({}, {features::kOrcaInternationalizeElaborate});

  orca::mojom::EditorConfigPtr config = BuildConfigFor(language_category);

  EXPECT_THAT(
      config->allowed_query_types,
      UnorderedElementsAre(orca::mojom::PresetTextQueryType::kShorten,
                           orca::mojom::PresetTextQueryType::kRephrase,
                           orca::mojom::PresetTextQueryType::kFormalize,
                           orca::mojom::PresetTextQueryType::kEmojify,
                           orca::mojom::PresetTextQueryType::kProofread));
}

TEST_P(InternationalizedCases, WithRephraseDisabled) {
  const LanguageCategory& language_category = GetParam();
  ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({}, {features::kOrcaInternationalizeRephrase});

  orca::mojom::EditorConfigPtr config = BuildConfigFor(language_category);

  EXPECT_THAT(
      config->allowed_query_types,
      UnorderedElementsAre(orca::mojom::PresetTextQueryType::kShorten,
                           orca::mojom::PresetTextQueryType::kElaborate,
                           orca::mojom::PresetTextQueryType::kFormalize,
                           orca::mojom::PresetTextQueryType::kEmojify,
                           orca::mojom::PresetTextQueryType::kProofread));
}

TEST_P(InternationalizedCases, WithFormalizeDisabled) {
  const LanguageCategory& language_category = GetParam();
  ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({}, {features::kOrcaInternationalizeFormalize});

  orca::mojom::EditorConfigPtr config = BuildConfigFor(language_category);

  EXPECT_THAT(
      config->allowed_query_types,
      UnorderedElementsAre(orca::mojom::PresetTextQueryType::kShorten,
                           orca::mojom::PresetTextQueryType::kElaborate,
                           orca::mojom::PresetTextQueryType::kRephrase,
                           orca::mojom::PresetTextQueryType::kEmojify,
                           orca::mojom::PresetTextQueryType::kProofread));
}

TEST_P(InternationalizedCases, WithEmojifyDisabled) {
  const LanguageCategory& language_category = GetParam();
  ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({}, {features::kOrcaInternationalizeEmojify});

  orca::mojom::EditorConfigPtr config = BuildConfigFor(language_category);

  EXPECT_THAT(
      config->allowed_query_types,
      UnorderedElementsAre(orca::mojom::PresetTextQueryType::kShorten,
                           orca::mojom::PresetTextQueryType::kElaborate,
                           orca::mojom::PresetTextQueryType::kRephrase,
                           orca::mojom::PresetTextQueryType::kFormalize,
                           orca::mojom::PresetTextQueryType::kProofread));
}

TEST_P(InternationalizedCases, WithProofreadDisabled) {
  const LanguageCategory& language_category = GetParam();
  ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({}, {features::kOrcaInternationalizeProofread});

  orca::mojom::EditorConfigPtr config = BuildConfigFor(language_category);

  EXPECT_THAT(config->allowed_query_types,
              UnorderedElementsAre(orca::mojom::PresetTextQueryType::kShorten,
                                   orca::mojom::PresetTextQueryType::kElaborate,
                                   orca::mojom::PresetTextQueryType::kRephrase,
                                   orca::mojom::PresetTextQueryType::kFormalize,
                                   orca::mojom::PresetTextQueryType::kEmojify));
}

}  // namespace
}  // namespace ash::input_method
