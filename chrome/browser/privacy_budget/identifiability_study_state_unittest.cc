// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_budget/identifiability_study_state.h"
#include <math.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/containers/flat_tree.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/privacy_budget/inspectable_identifiability_study_state.h"
#include "chrome/browser/privacy_budget/privacy_budget_prefs.h"
#include "chrome/common/privacy_budget/field_trial_param_conversions.h"
#include "chrome/common/privacy_budget/privacy_budget_features.h"
#include "chrome/common/privacy_budget/scoped_privacy_budget_config.h"
#include "chrome/common/privacy_budget/types.h"
#include "components/prefs/testing_pref_service.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"

namespace {

// Matcher that takes an IdentifiableSurface() as a parameter and checks if the
// argument is the corresponding RepresentativeSurface.
//
// Use as:
//
//     const auto kSurface = blink::IdentifiableSurface::(...);
//     const auto RepresentativeSurface representative_surface(...);
//
//     EXPECT_THAT(representative_surface, Represents(kSurface));
MATCHER_P(Represents, surface, "") {
  return arg.value() == surface;
}

using ::testing::AllOf;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::IsEmpty;
using ::testing::IsSupersetOf;
using ::testing::Not;
using ::testing::UnorderedElementsAre;

// Constants used to set up the test configuration.
constexpr int kTestingStudyGeneration = 58;

constexpr auto kRegularTypeId =
    blink::IdentifiableSurface::Type::kGenericFontLookup;
constexpr auto kBlockedSurface1 =
    blink::IdentifiableSurface::FromTypeAndToken(kRegularTypeId, 1);
constexpr auto kBlockedType1 =
    blink::IdentifiableSurface::Type::kCanvasReadback;
constexpr auto kInternalSurface = blink::IdentifiableSurface::FromTypeAndToken(
    blink::IdentifiableSurface::Type::kReservedInternal,
    3);

constexpr auto kTestingActiveSurfaceBudget = 40u;
constexpr auto kTestingExpectedSurfaceCount = 1u;

// Sample surfaces. "Regular" means that the surface is not blocked and its type
// is not blocked.
constexpr auto kRegularSurface1 =
    blink::IdentifiableSurface::FromTypeAndToken(kRegularTypeId, 3);
constexpr auto kRegularSurface2 =
    blink::IdentifiableSurface::FromTypeAndToken(kRegularTypeId, 4);
constexpr auto kRegularSurface3 =
    blink::IdentifiableSurface::FromTypeAndToken(kRegularTypeId, 5);

constexpr auto kBlockedTypeSurface1 =
    blink::IdentifiableSurface::FromTypeAndToken(kBlockedType1, 1);  // = 258

// Invokes ShouldRecordSurface() on `state` for `surface_count` distinct
// surfaces. All the surfaces belong to the type kRegularTypeId with inputs
// in [0, surface_count).
int SimulateSurfaceSelectionRound(IdentifiabilityStudyState& state,
                                  int surface_count) {
  int selected_surface_count = 0;
  for (int i = 0; i < surface_count; ++i) {
    auto surface =
        blink::IdentifiableSurface::FromTypeAndToken(kRegularTypeId, i);
    if (state.ShouldRecordSurface(surface))
      ++selected_surface_count;
  }
  return selected_surface_count;
}

// Creates a list of distinct IdentifiableSurfaces with type kWebFeature and
// inputs in the range [offset, offset+count).
IdentifiableSurfaceList CreateSurfaceList(size_t count, size_t offset = 0) {
  IdentifiableSurfaceList list;
  list.reserve(count);
  for (auto token = 0u; token < count; ++token) {
    list.push_back(blink::IdentifiableSurface::FromTypeAndToken(
        blink::IdentifiableSurface::Type::kWebFeature, token + offset));
  }
  return list;
}

}  // namespace

class IdentifiabilityStudyStateTest : public ::testing::Test {
 public:
  IdentifiabilityStudyStateTest() {
    config_parameters_.generation = kTestingStudyGeneration;
    config_parameters_.blocked_surfaces.push_back(kBlockedSurface1);
    config_parameters_.blocked_types.push_back(kBlockedType1);
    config_parameters_.active_surface_budget = kTestingActiveSurfaceBudget;
    config_parameters_.expected_surface_count = kTestingExpectedSurfaceCount;
    config_.Apply(config_parameters_);
    prefs::RegisterPrivacyBudgetPrefs(pref_service_.registry());
    pref_service()->SetInteger(prefs::kPrivacyBudgetGeneration,
                               kTestingStudyGeneration);
    scoped_feature_list_.InitAndDisableFeature(
        features::kIdentifiabilityStudyMetaExperiment);
  }

  TestingPrefServiceSimple* pref_service() { return &pref_service_; }

 private:
  TestingPrefServiceSimple pref_service_;
  test::ScopedPrivacyBudgetConfig::Parameters config_parameters_;
  test::ScopedPrivacyBudgetConfig config_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST(IdentifiabilityStudyStateStandaloneTest, InstantiateAndInitialize) {
  test::ScopedPrivacyBudgetConfig config(
      test::ScopedPrivacyBudgetConfig::Presets::kEnableRandomSampling);

  TestingPrefServiceSimple pref_service;
  prefs::RegisterPrivacyBudgetPrefs(pref_service.registry());

  // Default is zero.
  ASSERT_EQ(0, pref_service.GetInteger(prefs::kPrivacyBudgetGeneration));
  auto settings = std::make_unique<IdentifiabilityStudyState>(&pref_service);

  // Successful initialization should result in setting the generation number.
  EXPECT_EQ(test::ScopedPrivacyBudgetConfig::kDefaultGeneration,
            pref_service.GetInteger(prefs::kPrivacyBudgetGeneration));
  // There should be at least one offset selected.
  EXPECT_FALSE(
      pref_service.GetString(prefs::kPrivacyBudgetSelectedOffsets).empty());
  // But there should be no seen surfaces.
  EXPECT_TRUE(
      pref_service.GetString(prefs::kPrivacyBudgetSeenSurfaces).empty());
}

TEST_F(IdentifiabilityStudyStateTest, ReInitializesWhenGenerationChanges) {
  pref_service()->SetInteger(prefs::kPrivacyBudgetGeneration,
                             kTestingStudyGeneration - 1);
  pref_service()->SetString(
      prefs::kPrivacyBudgetSeenSurfaces,
      EncodeIdentifiabilityFieldTrialParam(IdentifiableSurfaceList{
          blink::IdentifiableSurface::FromMetricHash(4),
          blink::IdentifiableSurface::FromMetricHash(5),
          blink::IdentifiableSurface::FromMetricHash(6)}));

  auto settings = std::make_unique<IdentifiabilityStudyState>(pref_service());

  EXPECT_EQ(kTestingStudyGeneration,
            pref_service()->GetInteger(prefs::kPrivacyBudgetGeneration));
  EXPECT_THAT(pref_service()->GetString(prefs::kPrivacyBudgetSelectedOffsets),
              Not(IsEmpty()));
  EXPECT_THAT(pref_service()->GetString(prefs::kPrivacyBudgetSeenSurfaces),
              IsEmpty());
}

TEST_F(IdentifiabilityStudyStateTest,
       LoadsSeenSurfacesAndSelectedOffsetsFromPrefs) {
  auto kSurfaces = IdentifiableSurfaceList{kRegularSurface1, kRegularSurface2};
  pref_service()->SetString(prefs::kPrivacyBudgetSeenSurfaces,
                            EncodeIdentifiabilityFieldTrialParam(kSurfaces));

  auto kOffsets = std::vector<IdentifiabilityStudyState::OffsetType>{0, 1};
  pref_service()->SetString(prefs::kPrivacyBudgetSelectedOffsets,
                            EncodeIdentifiabilityFieldTrialParam(kOffsets));
  auto state =
      std::make_unique<test_utils::InspectableIdentifiabilityStudyState>(
          pref_service());
  EXPECT_THAT(state->active_surfaces(),
              UnorderedElementsAre(Represents(kRegularSurface1),
                                   Represents(kRegularSurface2)));
  EXPECT_THAT(state->seen_surfaces().AsList(), ElementsAreArray(kSurfaces));
  EXPECT_THAT(state->selected_offsets(), IsSupersetOf(kOffsets));
  EXPECT_EQ(kTestingStudyGeneration, state->generation());
}

// Valid offsets, as decided by IsValidOffset() go from 0 thru
// kMaxSelectedSurfaceOffset. Encountering invalid ones is a good sign that the
// prefs are inconsistent.
TEST_F(IdentifiabilityStudyStateTest, RecoversFromInvalidOffsets) {
  const std::vector<unsigned int> kBadOffsets = {
      5, 5, 6, 3, IdentifiabilityStudyState::kMaxSelectedSurfaceOffset + 1,
      2, 0, 5};

  pref_service()->SetString(prefs::kPrivacyBudgetSelectedOffsets,
                            EncodeIdentifiabilityFieldTrialParam(kBadOffsets));
  const IdentifiableSurfaceList kSeenSurfaces = {
      kRegularSurface1, kRegularSurface2, kRegularSurface3};
  pref_service()->SetString(
      prefs::kPrivacyBudgetSeenSurfaces,
      EncodeIdentifiabilityFieldTrialParam(kSeenSurfaces));

  auto state =
      std::make_unique<test_utils::InspectableIdentifiabilityStudyState>(
          pref_service());

  // The seen surfaces should've been dumped due to having invalid persisted
  // state.
  EXPECT_TRUE(state->seen_surfaces().empty());

  // Upon re-initialization there should be at least one selected offset to get
  // started with.
  EXPECT_FALSE(state->selected_offsets().empty());
}

// Any blocked surfaces in the seen surface list should be dropped. Selected
// offsets should be adjusted correspondingly.
TEST_F(IdentifiabilityStudyStateTest, DropsBlockedSurfaces_Suffix) {
  // The suffix of the seen surfaces list consists of blocked surfaces.
  pref_service()->SetString(
      prefs::kPrivacyBudgetSeenSurfaces,
      EncodeIdentifiabilityFieldTrialParam(IdentifiableSurfaceList{
          kRegularSurface1, kRegularSurface2, kRegularSurface3,
          kBlockedSurface1, kBlockedTypeSurface1}));
  pref_service()->SetString(prefs::kPrivacyBudgetSelectedOffsets, "0,1,3,4");
  auto settings =
      std::make_unique<test_utils::InspectableIdentifiabilityStudyState>(
          pref_service());
  EXPECT_THAT(settings->selected_offsets(),
              UnorderedElementsAre(IdentifiabilityStudyState::OffsetType{0},
                                   IdentifiabilityStudyState::OffsetType{1}));
  EXPECT_THAT(
      settings->active_surfaces(),
      ElementsAre(Represents(kRegularSurface1), Represents(kRegularSurface2)));
}

// Any blocked surfaces in the seen surface list should be dropped. Selected
// offsets should be adjusted correspondingly.
TEST_F(IdentifiabilityStudyStateTest, DropsBlockedSurfaces_Prefix) {
  // The prefix of the seen surfaces list consists of blocked surfaces.
  pref_service()->SetString(
      prefs::kPrivacyBudgetSeenSurfaces,
      EncodeIdentifiabilityFieldTrialParam(
          IdentifiableSurfaceList{kBlockedSurface1, kRegularSurface1,
                                  kRegularSurface2, kRegularSurface3}));
  pref_service()->SetString(prefs::kPrivacyBudgetSelectedOffsets, "1,3");
  auto settings =
      std::make_unique<test_utils::InspectableIdentifiabilityStudyState>(
          pref_service());
  EXPECT_THAT(settings->selected_offsets(), UnorderedElementsAre(0, 2));
  EXPECT_THAT(
      settings->active_surfaces(),
      ElementsAre(Represents(kRegularSurface1), Represents(kRegularSurface3)));
}

// Any blocked surfaces in the seen surface list should be dropped. Selected
// offsets should be adjusted correspondingly.
TEST_F(IdentifiabilityStudyStateTest, DropsBlockedSurfaces_Infix) {
  pref_service()->SetString(
      prefs::kPrivacyBudgetSeenSurfaces,
      EncodeIdentifiabilityFieldTrialParam(
          IdentifiableSurfaceList{kRegularSurface1, kBlockedSurface1,
                                  kRegularSurface2, kRegularSurface3}));
  pref_service()->SetString(prefs::kPrivacyBudgetSelectedOffsets,
                            "0,1,2,3,4,5");
  auto settings =
      std::make_unique<test_utils::InspectableIdentifiabilityStudyState>(
          pref_service());
  EXPECT_THAT(settings->selected_offsets(),
              UnorderedElementsAre(0, 1, 2, 3, 4));
  EXPECT_THAT(
      settings->active_surfaces(),
      ElementsAre(Represents(kRegularSurface1), Represents(kRegularSurface2),
                  Represents(kRegularSurface3)));
}

TEST_F(IdentifiabilityStudyStateTest, DropsBlockedTypes) {
  pref_service()->SetString(
      prefs::kPrivacyBudgetSeenSurfaces,
      EncodeIdentifiabilityFieldTrialParam(
          IdentifiableSurfaceList{kRegularSurface1, kBlockedTypeSurface1,
                                  kRegularSurface2, kRegularSurface3}));
  pref_service()->SetString(prefs::kPrivacyBudgetSelectedOffsets,
                            "0,1,2,3,4,5");
  auto settings =
      std::make_unique<test_utils::InspectableIdentifiabilityStudyState>(
          pref_service());
  EXPECT_THAT(
      settings->active_surfaces(),
      ElementsAre(Represents(kRegularSurface1), Represents(kRegularSurface2),
                  Represents(kRegularSurface3)));
  EXPECT_THAT(settings->selected_offsets(),
              AllOf(Contains(0u), Contains(1u), Contains(2u), Contains(3u),
                    Contains(4u)));
}

TEST_F(IdentifiabilityStudyStateTest, AllowsValidPersistedSurfaces) {
  pref_service()->SetString(
      prefs::kPrivacyBudgetSeenSurfaces,
      EncodeIdentifiabilityFieldTrialParam(IdentifiableSurfaceList{
          kRegularSurface1, kRegularSurface2, kRegularSurface3}));
  pref_service()->SetString(prefs::kPrivacyBudgetSelectedOffsets,
                            "0,1,2,3,4,5");
  auto settings =
      std::make_unique<test_utils::InspectableIdentifiabilityStudyState>(
          pref_service());
  EXPECT_TRUE(settings->ShouldRecordSurface(kRegularSurface1));
  EXPECT_TRUE(settings->ShouldRecordSurface(kRegularSurface2));
  EXPECT_TRUE(settings->ShouldRecordSurface(kRegularSurface3));
  EXPECT_THAT(settings->active_surfaces(),
              ::testing::ElementsAre(Represents(kRegularSurface1),
                                     Represents(kRegularSurface2),
                                     Represents(kRegularSurface3)));
}

// Different from the DropsBlockedSurfaces* tests in that the behavior being
// verified is that of ShouldRecordSurface().
TEST_F(IdentifiabilityStudyStateTest, DisallowsBlockedSurfaces) {
  pref_service()->SetString(
      prefs::kPrivacyBudgetSeenSurfaces,
      EncodeIdentifiabilityFieldTrialParam(IdentifiableSurfaceList{
          kRegularSurface1, kRegularSurface2, kRegularSurface3}));
  pref_service()->SetString(prefs::kPrivacyBudgetSelectedOffsets,
                            "0,1,2,3,4,5");
  auto settings =
      std::make_unique<test_utils::InspectableIdentifiabilityStudyState>(
          pref_service());
  EXPECT_FALSE(settings->ShouldRecordSurface(kBlockedSurface1));
  EXPECT_FALSE(settings->ShouldRecordSurface(kBlockedTypeSurface1));
}

TEST_F(IdentifiabilityStudyStateTest, UpdatesSeenSurfaces) {
  // The list of seen surfaces should be updated when a new surface was
  // observed.
  auto settings =
      std::make_unique<test_utils::InspectableIdentifiabilityStudyState>(
          pref_service());
  settings->SelectAllOffsetsForTesting();
  EXPECT_TRUE(settings->active_surfaces().Empty());
  EXPECT_THAT(settings->seen_surfaces(), IsEmpty());
  EXPECT_TRUE(settings->ShouldRecordSurface(kRegularSurface1));
  EXPECT_THAT(settings->active_surfaces().Container(),
              ElementsAre(Represents(kRegularSurface1)));
  EXPECT_THAT(settings->seen_surfaces().AsList(),
              ElementsAre(kRegularSurface1));
  EXPECT_EQ(pref_service()->GetString(prefs::kPrivacyBudgetSeenSurfaces),
            std::string("772"));
}

// If there are duplicate `seen` surfaces, then IdentifiabilityStudyState
// should drop the existing persisted state and re-initialize the study. An
// inconsistency in the persisted state should be considered irrecoverable.
TEST_F(IdentifiabilityStudyStateTest, DropsInvalidPrefs_DuplicateSurfaces) {
  pref_service()->SetString(
      prefs::kPrivacyBudgetSeenSurfaces,
      EncodeIdentifiabilityFieldTrialParam(
          IdentifiableSurfaceList{kRegularSurface1, kRegularSurface1}));

  auto settings =
      std::make_unique<test_utils::InspectableIdentifiabilityStudyState>(
          pref_service());
  EXPECT_TRUE(settings->seen_surfaces().empty());
}

// If there are `seen` surfaces that are internal metadata types, then
// IdentifiabilityStudyState should drop the existing persisted state and
// re-initialize the study. An inconsistency in the persisted state should be
// considered irrecoverable.
TEST_F(IdentifiabilityStudyStateTest, DropsInvalidPrefs_InternalTypes) {
  pref_service()->SetString(
      prefs::kPrivacyBudgetSeenSurfaces,
      EncodeIdentifiabilityFieldTrialParam(
          IdentifiableSurfaceList{blink::IdentifiableSurface::FromTypeAndToken(
              blink::IdentifiableSurface::Type::kReservedInternal, 1)}));

  auto settings =
      std::make_unique<test_utils::InspectableIdentifiabilityStudyState>(
          pref_service());
  EXPECT_TRUE(settings->seen_surfaces().empty());
}

// If there's not enough offsets selected, IdentifiabilityStudyState should
// select more. If any of the newly selected offsets are within the range of
// the seen surfaces, those seen surfaces should be automatically promoted to
// active surfaces.
TEST(IdentifiabilityStudyStateStandaloneTest,
     OffsetSelectionPromotesSeenToActiveSurfaces) {
  auto params = test::ScopedPrivacyBudgetConfig::Parameters{};
  params.active_surface_budget = 40u;
  params.expected_surface_count = params.active_surface_budget * 2;
  test::ScopedPrivacyBudgetConfig config(params);

  TestingPrefServiceSimple pref_service;
  prefs::RegisterPrivacyBudgetPrefs(pref_service.registry());

  IdentifiableSurfaceList seen_surfaces;
  constexpr unsigned kTargetOffsetCount =
      test_utils::InspectableIdentifiabilityStudyState::
          kMaxSelectedSurfaceOffset +
      1;
  for (uint64_t i = 0; i < kTargetOffsetCount; ++i) {
    seen_surfaces.push_back(blink::IdentifiableSurface::FromTypeAndToken(
        blink::IdentifiableSurface::Type::kCanvasReadback, i));
  }
  pref_service.SetString(prefs::kPrivacyBudgetSeenSurfaces,
                         EncodeIdentifiabilityFieldTrialParam(seen_surfaces));
  pref_service.SetInteger(prefs::kPrivacyBudgetGeneration, params.generation);

  auto settings =
      std::make_unique<test_utils::InspectableIdentifiabilityStudyState>(
          &pref_service);

  // The default costing model counts each surface as a single unit. So
  // params.active_surface_budget is also the maximum number of surfaces that
  // could be selected.
  EXPECT_EQ(static_cast<unsigned>(params.active_surface_budget),
            settings->active_surfaces().Size());
  EXPECT_EQ(kTargetOffsetCount, settings->seen_surfaces().size());
  EXPECT_EQ(static_cast<unsigned>(params.active_surface_budget),
            settings->selected_offsets().size());
}

// Verify that the study parameters don't overflow.
TEST(IdentifiabilityStudyStateStandaloneTest, HighClamps) {
  auto params = test::ScopedPrivacyBudgetConfig::Parameters{};
  params.active_surface_budget =
      features::kMaxIdentifiabilityStudyActiveSurfaceBudget + 1;
  params.expected_surface_count =
      features::kMaxIdentifiabilityStudyExpectedSurfaceCount + 1;
  test::ScopedPrivacyBudgetConfig config(params);

  TestingPrefServiceSimple pref_service;
  prefs::RegisterPrivacyBudgetPrefs(pref_service.registry());
  test_utils::InspectableIdentifiabilityStudyState settings(&pref_service);

  EXPECT_EQ(features::kMaxIdentifiabilityStudyActiveSurfaceBudget,
            settings.active_surface_budget());
}

// Verify that the study parameters don't underflow.
TEST(IdentifiabilityStudyStateStandaloneTest, LowClamps) {
  auto params = test::ScopedPrivacyBudgetConfig::Parameters{};
  params.active_surface_budget = -1;
  params.expected_surface_count = -1;
  test::ScopedPrivacyBudgetConfig config(params);

  TestingPrefServiceSimple pref_service;
  prefs::RegisterPrivacyBudgetPrefs(pref_service.registry());
  test_utils::InspectableIdentifiabilityStudyState settings(&pref_service);

  EXPECT_EQ(0, settings.active_surface_budget());
}

TEST(IdentifiabilityStudyStateStandaloneTest, Disabled) {
  test::ScopedPrivacyBudgetConfig config(
      test::ScopedPrivacyBudgetConfig::Presets::kDisable);

  TestingPrefServiceSimple pref_service;
  prefs::RegisterPrivacyBudgetPrefs(pref_service.registry());
  test_utils::InspectableIdentifiabilityStudyState settings(&pref_service);

  // The specific surface doesn't matter.
  EXPECT_FALSE(settings.ShouldRecordSurface(kRegularSurface1));
}

TEST(IdentifiabilityStudyStateStandaloneTest, ShouldReportEncounteredSurface) {
  test::ScopedPrivacyBudgetConfig config(
      test::ScopedPrivacyBudgetConfig::Presets::kEnableRandomSampling);

  TestingPrefServiceSimple pref_service;
  prefs::RegisterPrivacyBudgetPrefs(pref_service.registry());
  test_utils::InspectableIdentifiabilityStudyState state(&pref_service);

  EXPECT_TRUE(state.group_settings().enabled());
  EXPECT_TRUE(state.group_settings().IsUsingRandomSampling());

  // The specific surface doesn't matter.
  EXPECT_TRUE(state.ShouldReportEncounteredSurface(ukm::AssignNewSourceId(),
                                                   kRegularSurface1));
  EXPECT_TRUE(state.ShouldReportEncounteredSurface(ukm::NoURLSourceId(),
                                                   kRegularSurface1));
}

// Test the mode in which only the meta experiment (i.e. reporting encountered
// surfaces) is enabled.
TEST(IdentifiabilityStudyStateStandaloneTest, OnlyReportEncounteredSurface) {
  test::ScopedPrivacyBudgetConfig::Parameters params(
      test::ScopedPrivacyBudgetConfig::Presets::kEnableRandomSampling);
  params.allowed_random_types = {
      blink::IdentifiableSurface::Type::kReservedInternal};
  test::ScopedPrivacyBudgetConfig config(params);

  TestingPrefServiceSimple pref_service;
  prefs::RegisterPrivacyBudgetPrefs(pref_service.registry());
  test_utils::InspectableIdentifiabilityStudyState state(&pref_service);

  EXPECT_TRUE(state.group_settings().enabled());
  EXPECT_TRUE(state.group_settings().IsUsingRandomSampling());

  // The specific surface doesn't matter.
  EXPECT_TRUE(state.ShouldReportEncounteredSurface(ukm::AssignNewSourceId(),
                                                   kRegularSurface1));
  EXPECT_FALSE(state.ShouldRecordSurface(kRegularSurface1));
}

TEST(IdentifiabilityStudyStateStandaloneTest, ClearsPrefsIfStudyIsDisabled) {
  test::ScopedPrivacyBudgetConfig config(
      test::ScopedPrivacyBudgetConfig::Presets::kDisable);
  TestingPrefServiceSimple pref_service;
  prefs::RegisterPrivacyBudgetPrefs(pref_service.registry());
  pref_service.SetInteger(prefs::kPrivacyBudgetGeneration,
                          test::ScopedPrivacyBudgetConfig::kDefaultGeneration);
  pref_service.SetString(prefs::kPrivacyBudgetSeenSurfaces, "1,2,3");
  pref_service.SetString(prefs::kPrivacyBudgetSelectedOffsets, "100,200,300");

  IdentifiabilityStudyState state(&pref_service);

  EXPECT_EQ(0, pref_service.GetInteger(prefs::kPrivacyBudgetGeneration));
  EXPECT_THAT(pref_service.GetString(prefs::kPrivacyBudgetSeenSurfaces),
              IsEmpty());
  EXPECT_THAT(pref_service.GetString(prefs::kPrivacyBudgetSelectedOffsets),
              IsEmpty());
}

TEST_F(IdentifiabilityStudyStateTest, StripDisallowedSurfaces_Empty) {
  test_utils::InspectableIdentifiabilityStudyState state(pref_service());
  std::vector<IdentifiabilityStudyState::OffsetType> dropped;
  IdentifiableSurfaceList surfaces;
  EXPECT_TRUE(
      test_utils::InspectableIdentifiabilityStudyState::StripDisallowedSurfaces(
          surfaces, dropped));
  EXPECT_THAT(dropped, IsEmpty());
  EXPECT_THAT(surfaces, IsEmpty());
}

TEST_F(IdentifiabilityStudyStateTest, StripDisallowedSurfaces_SingleBad) {
  test_utils::InspectableIdentifiabilityStudyState state(pref_service());
  std::vector<IdentifiabilityStudyState::OffsetType> dropped;
  IdentifiableSurfaceList surfaces{kBlockedSurface1, kRegularSurface1};
  EXPECT_TRUE(
      test_utils::InspectableIdentifiabilityStudyState::StripDisallowedSurfaces(
          surfaces, dropped));
  EXPECT_THAT(dropped, ElementsAre(0));
  EXPECT_THAT(surfaces, ElementsAre(kRegularSurface1));
}

TEST_F(IdentifiabilityStudyStateTest, StripDisallowedSurfaces_BadSuffix) {
  test_utils::InspectableIdentifiabilityStudyState state(pref_service());
  std::vector<IdentifiabilityStudyState::OffsetType> dropped;
  IdentifiableSurfaceList surfaces{kRegularSurface1, kBlockedSurface1};
  EXPECT_TRUE(
      test_utils::InspectableIdentifiabilityStudyState::StripDisallowedSurfaces(
          surfaces, dropped));
  EXPECT_THAT(dropped, ElementsAre(1));
  EXPECT_THAT(surfaces, ElementsAre(kRegularSurface1));
}

TEST_F(IdentifiabilityStudyStateTest, StripDisallowedSurfaces_Duplicates) {
  test_utils::InspectableIdentifiabilityStudyState state(pref_service());
  std::vector<IdentifiabilityStudyState::OffsetType> dropped;
  IdentifiableSurfaceList surfaces{kRegularSurface1, kRegularSurface1};
  EXPECT_FALSE(
      test_utils::InspectableIdentifiabilityStudyState::StripDisallowedSurfaces(
          surfaces, dropped));
}

TEST_F(IdentifiabilityStudyStateTest, StripDisallowedSurfaces_InternalSurface) {
  test_utils::InspectableIdentifiabilityStudyState state(pref_service());
  std::vector<IdentifiabilityStudyState::OffsetType> dropped;
  IdentifiableSurfaceList surfaces{
      kRegularSurface1,
      blink::IdentifiableSurface::FromTypeAndToken(
          blink::IdentifiableSurface::Type::kReservedInternal, 1)};
  EXPECT_FALSE(
      test_utils::InspectableIdentifiabilityStudyState::StripDisallowedSurfaces(
          surfaces, dropped));
}

TEST(IdentifiabilityStudyStateStandaloneTest, AdjustForDroppedOffsets_Empty) {
  std::vector<IdentifiabilityStudyState::OffsetType> dropped;
  std::vector<IdentifiabilityStudyState::OffsetType> original;
  auto result =
      test_utils::InspectableIdentifiabilityStudyState::AdjustForDroppedOffsets(
          dropped, original);
  EXPECT_THAT(result, IsEmpty());
}

TEST(IdentifiabilityStudyStateStandaloneTest,
     AdjustForDroppedOffsets_NoDropped) {
  std::vector<IdentifiabilityStudyState::OffsetType> dropped;
  std::vector<IdentifiabilityStudyState::OffsetType> original{1, 5, 7, 9};
  auto result =
      test_utils::InspectableIdentifiabilityStudyState::AdjustForDroppedOffsets(
          dropped, original);
  EXPECT_THAT(result, ElementsAreArray(original));
}

TEST(IdentifiabilityStudyStateStandaloneTest,
     AdjustForDroppedOffsets_DroppedZero) {
  std::vector<IdentifiabilityStudyState::OffsetType> dropped{0};
  std::vector<IdentifiabilityStudyState::OffsetType> original{1, 5, 7, 9};
  auto result =
      test_utils::InspectableIdentifiabilityStudyState::AdjustForDroppedOffsets(
          dropped, original);
  // All offsets in `original` should be decreased by 1.
  EXPECT_THAT(result, ElementsAre(0, 4, 6, 8));
}

TEST(IdentifiabilityStudyStateStandaloneTest,
     AdjustForDroppedOffsets_DroppedOne) {
  std::vector<IdentifiabilityStudyState::OffsetType> dropped{1};
  std::vector<IdentifiabilityStudyState::OffsetType> original{1, 5, 7, 9};
  auto result =
      test_utils::InspectableIdentifiabilityStudyState::AdjustForDroppedOffsets(
          dropped, original);
  EXPECT_THAT(result, ElementsAre(4, 6, 8));
}

TEST(IdentifiabilityStudyStateStandaloneTest,
     AdjustForDroppedOffsets_DroppedMultiple) {
  std::vector<IdentifiabilityStudyState::OffsetType> dropped{2, 3, 5};
  std::vector<IdentifiabilityStudyState::OffsetType> original{1, 2, 4, 6};
  auto result =
      test_utils::InspectableIdentifiabilityStudyState::AdjustForDroppedOffsets(
          dropped, original);
  EXPECT_THAT(result, ElementsAre(1, 2, 3));
}

TEST(IdentifiabilityStudyStateStandaloneTest, OffsetUsesUpAllTheBudget) {
  test::ScopedPrivacyBudgetConfig::Parameters parameters;
  parameters.expected_surface_count = kTestingExpectedSurfaceCount;
  parameters.active_surface_budget = kTestingActiveSurfaceBudget;
  // kRegularSurface2 costs the entire budget.
  parameters.per_surface_cost = {
      {kRegularSurface1, 1.0},
      {kRegularSurface2, parameters.active_surface_budget - 1},
      {kRegularSurface3, 1.0}};
  test::ScopedPrivacyBudgetConfig config(parameters);

  TestingPrefServiceSimple pref_service;
  prefs::RegisterPrivacyBudgetPrefs(pref_service.registry());
  pref_service.SetInteger(prefs::kPrivacyBudgetGeneration,
                          parameters.generation);
  pref_service.SetString(prefs::kPrivacyBudgetSelectedOffsets, "0,1,2");

  test_utils::InspectableIdentifiabilityStudyState state(&pref_service);

  EXPECT_TRUE(state.ShouldRecordSurface(kRegularSurface1));
  EXPECT_TRUE(state.ShouldRecordSurface(kRegularSurface2));
  EXPECT_FALSE(state.ShouldRecordSurface(kRegularSurface3));

  // kRegularSurface3 doesn't end up in the seen_surface_sequence because we've
  // saturated the active surface budget and are no longer interested in new
  // surfaces.
  EXPECT_THAT(state.seen_surfaces(),
              ElementsAre(kRegularSurface1, kRegularSurface2));
}

TEST(IdentifiabilityStudyStateStandaloneTest, NextOffsetIsTooExpensive) {
  test::ScopedPrivacyBudgetConfig::Parameters parameters;
  parameters.expected_surface_count = kTestingExpectedSurfaceCount;
  parameters.active_surface_budget = kTestingActiveSurfaceBudget;
  // kRegularSurface1 costs the entire budget.
  parameters.per_surface_cost = {
      {kRegularSurface1, parameters.active_surface_budget + 1}};
  test::ScopedPrivacyBudgetConfig config(parameters);

  TestingPrefServiceSimple pref_service;
  prefs::RegisterPrivacyBudgetPrefs(pref_service.registry());
  pref_service.SetInteger(prefs::kPrivacyBudgetGeneration,
                          parameters.generation);
  pref_service.SetString(prefs::kPrivacyBudgetSelectedOffsets, "0");

  test_utils::InspectableIdentifiabilityStudyState state(&pref_service);

  // Selected offset `0` is from prefs.
  EXPECT_THAT(state.selected_offsets(), Contains(0));

  // This surface is too expensive, which should result in `state` dropping the
  // corresponding offset.
  EXPECT_FALSE(state.ShouldRecordSurface(kRegularSurface1));
  EXPECT_THAT(state.selected_offsets(), Not(Contains(0)));
  EXPECT_THAT(state.seen_surfaces(), ElementsAre(kRegularSurface1));
}

// TODO(crbug.com/40888212): Flaky on all platforms
TEST(IdentifiabilityStudyStateStandaloneTest, DISABLED_ReachesPivotPoint) {
  test::ScopedPrivacyBudgetConfig::Parameters parameters;
  parameters.active_surface_budget = kTestingActiveSurfaceBudget;

  // Try out 10x the number of surfaces we want to select.
  constexpr auto kExpectedSurfaceCount = kTestingActiveSurfaceBudget * 10;
  parameters.expected_surface_count = kExpectedSurfaceCount;
  test::ScopedPrivacyBudgetConfig config(parameters);

  int active_surface_count = 0;

  constexpr int kTrials = 10;

  for (auto trials = 0; trials < kTrials; ++trials) {
    TestingPrefServiceSimple pref_service;
    prefs::RegisterPrivacyBudgetPrefs(pref_service.registry());
    test_utils::InspectableIdentifiabilityStudyState state(&pref_service);
    active_surface_count +=
        SimulateSurfaceSelectionRound(state, kExpectedSurfaceCount);
  }

  // If we see `expected_surface_count` many surfaces, then we *should* consume
  // roughly `active_surface_budget` * `kMesaDistributionRatio` surfaces. (the
  // budget that's allocated for the ordinal space between 0 and the pivot
  // point.)
  constexpr double kExpectedActiveSurfaceCount =
      kTestingActiveSurfaceBudget *
      IdentifiabilityStudyState::kMesaDistributionRatio;

  const double kEpsilon =
      kExpectedActiveSurfaceCount * 0.05 /* 5% margin of error */;

  EXPECT_NEAR(kExpectedActiveSurfaceCount, active_surface_count * 1.0 / kTrials,
              kEpsilon);
}

TEST(IdentifiabilityStudyStateStandaloneTest, CheapSurfaces) {
  // For this test, we are going to use surfaces that cost
  // 1/kSurfacesPerCostUnit units. The objective is to ensure that the code
  // correctly handles the case where the number of surfaces that saturate the
  // budget is substantially larger than the budget itself.
  constexpr auto kSurfacesPerCostUnit = 4;

  // The number of surfaces that are selected must not exceed this value.
  constexpr auto kMaxSelectedSurfaces =
      kTestingActiveSurfaceBudget * kSurfacesPerCostUnit + 1 -
      kSurfacesPerCostUnit;

  // Expect a substantially larger number of surfaces than the number we expect
  // to select.
  constexpr auto kExpectedSurfaceCount = kMaxSelectedSurfaces * 10;

  test::ScopedPrivacyBudgetConfig::Parameters parameters;
  parameters.active_surface_budget = kTestingActiveSurfaceBudget;
  parameters.expected_surface_count = kExpectedSurfaceCount;
  parameters.per_type_cost = {
      {kRegularTypeId, 1.0 / kSurfacesPerCostUnit},
  };
  test::ScopedPrivacyBudgetConfig config(parameters);

  TestingPrefServiceSimple pref_service;
  prefs::RegisterPrivacyBudgetPrefs(pref_service.registry());
  test_utils::InspectableIdentifiabilityStudyState state(&pref_service);
  // Invoking ShouldRecordSurface() for kExpectedSurfaceCount surfaces should
  // result in at least
  unsigned selected_surfaces =
      SimulateSurfaceSelectionRound(state, kExpectedSurfaceCount);
  ASSERT_LE(selected_surfaces, kMaxSelectedSurfaces);

  EXPECT_GT(selected_surfaces, kTestingActiveSurfaceBudget);
  EXPECT_LE(selected_surfaces, kMaxSelectedSurfaces);
}

TEST(IdentifiabilityStudyStateStandaloneTest,
     AlwaysSampleReservedInternalRandom) {
  test::ScopedPrivacyBudgetConfig::Parameters parameters;
  parameters.active_surface_budget = kTestingActiveSurfaceBudget;
  parameters.expected_surface_count = 20;
  parameters.allowed_random_types = {
      blink::IdentifiableSurface::Type::kWebFeature};
  test::ScopedPrivacyBudgetConfig config(parameters);

  TestingPrefServiceSimple pref_service;
  prefs::RegisterPrivacyBudgetPrefs(pref_service.registry());
  test_utils::InspectableIdentifiabilityStudyState state(&pref_service);

  EXPECT_TRUE(
      state.ShouldRecordSurface(blink::IdentifiableSurface::FromTypeAndToken(
          blink::IdentifiableSurface::Type::kReservedInternal,
          blink::IdentifiableToken(
              blink::IdentifiableSurface::ReservedSurfaceMetrics::
                  kDocumentCreated_IsCrossOriginFrame))));
  EXPECT_TRUE(
      state.ShouldRecordSurface(blink::IdentifiableSurface::FromTypeAndToken(
          blink::IdentifiableSurface::Type::kReservedInternal,
          blink::IdentifiableToken(
              blink::IdentifiableSurface::ReservedSurfaceMetrics::
                  kDocumentCreated_IsCrossSiteFrame))));
  EXPECT_TRUE(
      state.ShouldRecordSurface(blink::IdentifiableSurface::FromTypeAndToken(
          blink::IdentifiableSurface::Type::kReservedInternal,
          blink::IdentifiableToken(
              blink::IdentifiableSurface::ReservedSurfaceMetrics::
                  kDocumentCreated_IsMainFrame))));
  EXPECT_TRUE(
      state.ShouldRecordSurface(blink::IdentifiableSurface::FromTypeAndToken(
          blink::IdentifiableSurface::Type::kReservedInternal,
          blink::IdentifiableToken(
              blink::IdentifiableSurface::ReservedSurfaceMetrics::
                  kDocumentCreated_NavigationSourceId))));
}

TEST(IdentifiabilityStudyStateStandaloneTest,
     AlwaysSampleReservedInternalBlock) {
  const int kTestGroupCount = 80;
  const int kSurfacesInGroup = 40;

  test::ScopedPrivacyBudgetConfig::Parameters parameters;
  for (int group_index = 0; group_index < kTestGroupCount; ++group_index) {
    parameters.blocks.emplace_back(
        CreateSurfaceList(kSurfacesInGroup, kSurfacesInGroup * group_index));
  }

  parameters.block_weights.assign(kTestGroupCount, 1.0);
  parameters.active_surface_budget = kSurfacesInGroup;
  test::ScopedPrivacyBudgetConfig config(parameters);

  TestingPrefServiceSimple pref_service;
  prefs::RegisterPrivacyBudgetPrefs(pref_service.registry());
  test_utils::InspectableIdentifiabilityStudyState state(&pref_service);

  EXPECT_TRUE(
      state.ShouldRecordSurface(blink::IdentifiableSurface::FromTypeAndToken(
          blink::IdentifiableSurface::Type::kReservedInternal,
          blink::IdentifiableToken(
              blink::IdentifiableSurface::ReservedSurfaceMetrics::
                  kDocumentCreated_IsCrossOriginFrame))));
  EXPECT_TRUE(
      state.ShouldRecordSurface(blink::IdentifiableSurface::FromTypeAndToken(
          blink::IdentifiableSurface::Type::kReservedInternal,
          blink::IdentifiableToken(
              blink::IdentifiableSurface::ReservedSurfaceMetrics::
                  kDocumentCreated_IsCrossSiteFrame))));
  EXPECT_TRUE(
      state.ShouldRecordSurface(blink::IdentifiableSurface::FromTypeAndToken(
          blink::IdentifiableSurface::Type::kReservedInternal,
          blink::IdentifiableToken(
              blink::IdentifiableSurface::ReservedSurfaceMetrics::
                  kDocumentCreated_IsMainFrame))));
  EXPECT_TRUE(
      state.ShouldRecordSurface(blink::IdentifiableSurface::FromTypeAndToken(
          blink::IdentifiableSurface::Type::kReservedInternal,
          blink::IdentifiableToken(
              blink::IdentifiableSurface::ReservedSurfaceMetrics::
                  kDocumentCreated_NavigationSourceId))));
}

TEST(IdentifiabilityStudyStateStandaloneTest, NoAllowedTypes) {
  constexpr auto kExpectedSurfaceCount = 20;

  test::ScopedPrivacyBudgetConfig::Parameters parameters;
  parameters.active_surface_budget = kTestingActiveSurfaceBudget;
  parameters.expected_surface_count = kExpectedSurfaceCount;
  parameters.allowed_random_types = {
      blink::IdentifiableSurface::Type::kWebFeature};
  test::ScopedPrivacyBudgetConfig config(parameters);

  TestingPrefServiceSimple pref_service;
  prefs::RegisterPrivacyBudgetPrefs(pref_service.registry());
  test_utils::InspectableIdentifiabilityStudyState state(&pref_service);
  // Invoking ShouldRecordSurface() for kExpectedSurfaceCount surfaces should
  // result in at least
  unsigned selected_surfaces =
      SimulateSurfaceSelectionRound(state, kExpectedSurfaceCount);
  // We only allow kWebFeature, but SimulateSurfaceSelectionRound is trying to
  // add kGenericFontLookup surfaces
  const unsigned int expected_surfaces = 0;
  EXPECT_EQ(selected_surfaces, expected_surfaces);
}

TEST(IdentifiabilityStudyStateStandaloneTest, OnlyAllowedTypes) {
  constexpr auto kExpectedSurfaceCount = 20;

  test::ScopedPrivacyBudgetConfig::Parameters parameters;
  parameters.active_surface_budget = kTestingActiveSurfaceBudget;
  parameters.expected_surface_count = kExpectedSurfaceCount;
  parameters.allowed_random_types = {
      blink::IdentifiableSurface::Type::kGenericFontLookup,
      blink::IdentifiableSurface::Type::kWebFeature};
  test::ScopedPrivacyBudgetConfig config(parameters);

  TestingPrefServiceSimple pref_service;
  prefs::RegisterPrivacyBudgetPrefs(pref_service.registry());
  test_utils::InspectableIdentifiabilityStudyState state(&pref_service);
  // Invoking ShouldRecordSurface() for kExpectedSurfaceCount surfaces should
  // result in at least
  unsigned selected_surfaces =
      SimulateSurfaceSelectionRound(state, kExpectedSurfaceCount);
  // We allow kWebFeature and kGenericFontLookup types, and
  // SimulateSurfaceSelectionRound is trying to add kGenericFontLookup surfaces
  // so we should sample at least one surface
  const unsigned int min_expected_surfaces = 1;
  EXPECT_GT(selected_surfaces, min_expected_surfaces);
}

TEST(IdentifiabilityStudyStateStandaloneTest, SomeSetOfGroups) {
  // Number of test groups. Arbitrary.
  constexpr unsigned kTestGroupCount = 80;

  // Number of surfaces in a single group. The groups don't need to be the same
  // size.
  constexpr unsigned kSurfacesInGroup = 40;

  test::ScopedPrivacyBudgetConfig::Parameters parameters;
  for (unsigned group_index = 0; group_index < kTestGroupCount; ++group_index) {
    parameters.blocks.emplace_back(
        CreateSurfaceList(kSurfacesInGroup, kSurfacesInGroup * group_index));
  }
  parameters.block_weights.assign(kTestGroupCount, 1.0);
  parameters.active_surface_budget = kSurfacesInGroup;
  test::ScopedPrivacyBudgetConfig config(parameters);

  TestingPrefServiceSimple pref_service;
  prefs::RegisterPrivacyBudgetPrefs(pref_service.registry());
  test_utils::InspectableIdentifiabilityStudyState state(&pref_service);

  // Any single selected group contributes kSurfacesInGroup surfaces.
  EXPECT_EQ(kSurfacesInGroup, state.active_surfaces().Size());
  EXPECT_EQ(0u, state.seen_surfaces().size());
}

// Regression test for the problem solved by https://crrev.com/c/3211286
TEST(IdentifiabilityStudyStateStandaloneTest,
     SomeSetOfGroupsWithRhoEqualToZero) {
  // Number of test groups. Arbitrary.
  constexpr unsigned kTestGroupCount = 80;

  // Number of surfaces in a single group. The groups don't need to be the same
  // size.
  constexpr unsigned kSurfacesInGroup = 40;

  test::ScopedPrivacyBudgetConfig::Parameters parameters;
  for (unsigned group_index = 0; group_index < kTestGroupCount; ++group_index) {
    parameters.blocks.emplace_back(
        CreateSurfaceList(kSurfacesInGroup, kSurfacesInGroup * group_index));
  }
  parameters.block_weights.assign(kTestGroupCount, 1.0);
  parameters.active_surface_budget = kSurfacesInGroup;
  parameters.expected_surface_count = 0;
  test::ScopedPrivacyBudgetConfig config(parameters);

  TestingPrefServiceSimple pref_service;
  prefs::RegisterPrivacyBudgetPrefs(pref_service.registry());
  test_utils::InspectableIdentifiabilityStudyState state(&pref_service);

  EXPECT_TRUE(state.group_settings().IsUsingAssignedBlockSampling());

  // Any single selected group contributes kSurfacesInGroup surfaces.
  EXPECT_EQ(kSurfacesInGroup, state.active_surfaces().Size());
}

TEST(IdentifiabilityStudyStateStandaloneTest, GroupsWithWeights) {
  constexpr unsigned kTestGroupCount = 80;
  constexpr unsigned kSurfacesInGroup = 40;

  test::ScopedPrivacyBudgetConfig::Parameters parameters;
  for (unsigned group_index = 0; group_index < kTestGroupCount; ++group_index) {
    parameters.blocks.emplace_back(
        CreateSurfaceList(kSurfacesInGroup, kSurfacesInGroup * group_index));
  }

  parameters.block_weights.assign(kTestGroupCount, 1.0);
  parameters.active_surface_budget = kSurfacesInGroup;
  test::ScopedPrivacyBudgetConfig config(parameters);

  TestingPrefServiceSimple pref_service;
  prefs::RegisterPrivacyBudgetPrefs(pref_service.registry());
  test_utils::InspectableIdentifiabilityStudyState state(&pref_service);

  int surfaces_to_record = 0;
  for (const IdentifiableSurfaceList& block : parameters.blocks) {
    if (state.ShouldRecordSurface(block[0]))
      surfaces_to_record++;
  }
  // Should record surfaces from exactly one group.
  EXPECT_EQ(1, surfaces_to_record);
}

TEST(IdentifiabilityStudyStateStandaloneTest, GroupSelectionPersists) {
  constexpr unsigned kTestGroupCount = 80;
  constexpr unsigned kSurfacesInGroup = 40;

  test::ScopedPrivacyBudgetConfig::Parameters parameters;
  for (unsigned group_index = 0; group_index < kTestGroupCount; ++group_index) {
    parameters.blocks.emplace_back(
        CreateSurfaceList(kSurfacesInGroup, kSurfacesInGroup * group_index));
  }
  parameters.block_weights.assign(kTestGroupCount, 1.0);
  parameters.active_surface_budget = kSurfacesInGroup;

  // Runs two scoped test. The selection from the first one should persist to
  // the next.
  TestingPrefServiceSimple pref_service;
  prefs::RegisterPrivacyBudgetPrefs(pref_service.registry());

  int selected_block_offset = -1;

  {
    test::ScopedPrivacyBudgetConfig config(parameters);
    test_utils::InspectableIdentifiabilityStudyState state(&pref_service);
    selected_block_offset = state.selected_block_offset();
    ASSERT_GE(selected_block_offset, 0);
    ASSERT_LT(static_cast<unsigned>(selected_block_offset),
              parameters.blocks.size());
    EXPECT_TRUE(
        state.ShouldRecordSurface(parameters.blocks[selected_block_offset][0]));
  }

  {
    test::ScopedPrivacyBudgetConfig config(parameters);
    test_utils::InspectableIdentifiabilityStudyState state(&pref_service);
    EXPECT_TRUE(
        state.ShouldRecordSurface(parameters.blocks[selected_block_offset][0]));
  }
}

TEST(IdentifiabilityStudyStateStandaloneTest, GroupOverflowsExperimentBudget) {
  constexpr unsigned kSurfacesInGroup = 40;
  // One unit short of accommodating the surface set.
  constexpr unsigned kActiveSurfaceBudget = kSurfacesInGroup - 1;

  test::ScopedPrivacyBudgetConfig::Parameters parameters;
  parameters.blocks.emplace_back(CreateSurfaceList(kSurfacesInGroup));
  parameters.active_surface_budget = kActiveSurfaceBudget;
  test::ScopedPrivacyBudgetConfig config(parameters);

  TestingPrefServiceSimple pref_service;
  prefs::RegisterPrivacyBudgetPrefs(pref_service.registry());
  test_utils::InspectableIdentifiabilityStudyState state(&pref_service);

  // Experiment is active, but there are no active surfaces.
  EXPECT_TRUE(state.active_surfaces().Empty());
}

TEST_F(IdentifiabilityStudyStateTest, WithAlsoMetaExperimentEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kIdentifiabilityStudyMetaExperiment,
      {{features::kIdentifiabilityStudyMetaExperimentActivationProbability.name,
        "1"}});

  TestingPrefServiceSimple pref_service;
  prefs::RegisterPrivacyBudgetPrefs(pref_service.registry());
  test_utils::InspectableIdentifiabilityStudyState settings(&pref_service);

  EXPECT_TRUE(settings.meta_experiment_active());
  // The specific surface doesn't matter.
  EXPECT_TRUE(settings.ShouldReportEncounteredSurface(ukm::AssignNewSourceId(),
                                                      kRegularSurface1));
  EXPECT_TRUE(settings.ShouldReportEncounteredSurface(ukm::AssignNewSourceId(),
                                                      kBlockedSurface1));
  EXPECT_TRUE(settings.ShouldReportEncounteredSurface(ukm::AssignNewSourceId(),
                                                      kBlockedTypeSurface1));
}

TEST(IdentifiabilityStudyStateStandaloneTest, MetaExperimentEnabled) {
  test::ScopedPrivacyBudgetConfig config(
      test::ScopedPrivacyBudgetConfig::Presets::kDisable);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kIdentifiabilityStudyMetaExperiment,
      {{features::kIdentifiabilityStudyMetaExperimentActivationProbability.name,
        "1"}});

  TestingPrefServiceSimple pref_service;
  prefs::RegisterPrivacyBudgetPrefs(pref_service.registry());
  test_utils::InspectableIdentifiabilityStudyState settings(&pref_service);

  EXPECT_TRUE(settings.meta_experiment_active());
  // The specific surface doesn't matter.
  EXPECT_TRUE(settings.ShouldReportEncounteredSurface(ukm::AssignNewSourceId(),
                                                      kRegularSurface1));
  EXPECT_TRUE(settings.ShouldReportEncounteredSurface(ukm::AssignNewSourceId(),
                                                      kBlockedSurface1));
  EXPECT_TRUE(settings.ShouldReportEncounteredSurface(ukm::AssignNewSourceId(),
                                                      kBlockedTypeSurface1));

  // Regular surfaces should be dropped, internal surfaces still recorded.
  EXPECT_TRUE(settings.ShouldRecordSurface(kInternalSurface));
  EXPECT_FALSE(settings.ShouldRecordSurface(kRegularSurface1));
}

TEST(IdentifiabilityStudyStateStandaloneTest, MetaExperimentDisabled) {
  test::ScopedPrivacyBudgetConfig config(
      test::ScopedPrivacyBudgetConfig::Presets::kDisable);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kIdentifiabilityStudyMetaExperiment);

  TestingPrefServiceSimple pref_service;
  prefs::RegisterPrivacyBudgetPrefs(pref_service.registry());
  test_utils::InspectableIdentifiabilityStudyState settings(&pref_service);

  EXPECT_FALSE(settings.meta_experiment_active());
  // The specific surface doesn't matter.
  EXPECT_FALSE(settings.ShouldReportEncounteredSurface(ukm::AssignNewSourceId(),
                                                       kRegularSurface1));
}

TEST(IdentifiabilityStudyStateStandaloneTest,
     MetaExperimentEnabledWithProbability) {
  test::ScopedPrivacyBudgetConfig config(
      test::ScopedPrivacyBudgetConfig::Presets::kDisable);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kIdentifiabilityStudyMetaExperiment,
      {{features::kIdentifiabilityStudyMetaExperimentActivationProbability.name,
        "0.3"}});

  int num_active = 0;
  for (int count = 0;; ++count) {
    TestingPrefServiceSimple pref_service;
    prefs::RegisterPrivacyBudgetPrefs(pref_service.registry());
    test_utils::InspectableIdentifiabilityStudyState settings(&pref_service);
    if (settings.meta_experiment_active()) {
      ++num_active;
    }
    if (count > 20 && std::abs(num_active - 0.3 * count) < 0.1) {
      break;
    }
  }
}

TEST(IdentifiabilityStudyStateStandaloneTest,
     MetaExperimentActivationStateStoredInPrefs) {
  test::ScopedPrivacyBudgetConfig config(
      test::ScopedPrivacyBudgetConfig::Presets::kDisable);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kIdentifiabilityStudyMetaExperiment,
      {{features::kIdentifiabilityStudyMetaExperimentActivationProbability.name,
        "0.5"}});

  TestingPrefServiceSimple pref_service;
  prefs::RegisterPrivacyBudgetPrefs(pref_service.registry());
  test_utils::InspectableIdentifiabilityStudyState settings(&pref_service);
  bool was_active = settings.meta_experiment_active();
  test_utils::InspectableIdentifiabilityStudyState new_settings(&pref_service);
  EXPECT_EQ(was_active, new_settings.meta_experiment_active());
}
