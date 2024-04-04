// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_budget/privacy_budget_ukm_entry_filter.h"

#include <cstdint>

#include "base/containers/flat_map.h"
#include "base/metrics/metrics_hashes.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/privacy_budget/identifiability_study_state.h"
#include "chrome/browser/privacy_budget/inspectable_identifiability_study_state.h"
#include "chrome/common/privacy_budget/privacy_budget_features.h"
#include "chrome/common/privacy_budget/scoped_privacy_budget_config.h"
#include "components/prefs/testing_pref_service.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/mojom/ukm_interface.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/utility/utility.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"

namespace {

using testing::IsSupersetOf;
using testing::Key;
using testing::Pair;
using testing::UnorderedElementsAre;

MATCHER_P(Type, type, "") {
  return blink::IdentifiableSurface::FromMetricHash(arg).GetType() == type;
}

TEST(PrivacyBudgetUkmEntryFilterStandaloneTest,
     BlocksIdentifiabilityMetricsByDefault) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kIdentifiabilityStudyMetaExperiment);
  TestingPrefServiceSimple pref_service;
  prefs::RegisterPrivacyBudgetPrefs(pref_service.registry());
  auto state =
      std::make_unique<test_utils::InspectableIdentifiabilityStudyState>(
          &pref_service);
  auto filter = std::make_unique<PrivacyBudgetUkmEntryFilter>(state.get());

  // By default the filter should reject all Identifiability events:
  base::flat_map<uint64_t, int64_t> events = {{1, 1}, {2, 2}};
  ukm::mojom::UkmEntryPtr x(
      std::in_place, 1, ukm::builders::Identifiability::kEntryNameHash, events);

  base::flat_set<uint64_t> filtered;
  EXPECT_FALSE(filter->FilterEntry(x.get(), &filtered));
  EXPECT_TRUE(filtered.empty());
}

TEST(PrivacyBudgetUkmEntryFilterStandaloneTest, AllowsOtherMetricsByDefault) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kIdentifiabilityStudyMetaExperiment);
  TestingPrefServiceSimple pref_service;
  prefs::RegisterPrivacyBudgetPrefs(pref_service.registry());
  auto state =
      std::make_unique<test_utils::InspectableIdentifiabilityStudyState>(
          &pref_service);
  auto filter = std::make_unique<PrivacyBudgetUkmEntryFilter>(state.get());

  base::flat_map<uint64_t, int64_t> events = {{1, 1}, {2, 2}};
  ukm::mojom::UkmEntryPtr x(std::in_place, 1,
                            ukm::builders::Blink_UseCounter::kEntryNameHash,
                            events);

  base::flat_set<uint64_t> filtered;
  EXPECT_TRUE(filter->FilterEntry(x.get(), &filtered));
  EXPECT_TRUE(filtered.empty());
  EXPECT_EQ(2u, x->metrics.size());
}

TEST(PrivacyBudgetUkmEntryFilterStandaloneTest, BlockListedMetrics) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kIdentifiabilityStudyMetaExperiment);

  constexpr uint64_t kBlockedSurface = 1;
  constexpr uint64_t kUnblockedSurface = 2;

  test::ScopedPrivacyBudgetConfig::Parameters parameters(
      test::ScopedPrivacyBudgetConfig::Presets::kEnableRandomSampling);
  parameters.blocked_surfaces = {
      blink::IdentifiableSurface::FromMetricHash(kBlockedSurface)};
  test::ScopedPrivacyBudgetConfig scoped_config(parameters);

  TestingPrefServiceSimple pref_service;
  prefs::RegisterPrivacyBudgetPrefs(pref_service.registry());
  auto state =
      std::make_unique<test_utils::InspectableIdentifiabilityStudyState>(
          &pref_service);
  state->SelectAllOffsetsForTesting();
  auto filter = std::make_unique<PrivacyBudgetUkmEntryFilter>(state.get());

  base::flat_map<uint64_t, int64_t> metrics = {{kBlockedSurface, 1},
                                               {kUnblockedSurface, 2}};
  base::flat_map<uint64_t, int64_t> expected_metrics = {
      {kUnblockedSurface, 2},
      {blink::IdentifiableSurface::FromTypeAndToken(
           blink::IdentifiableSurface::Type::kMeasuredSurface, 0)
           .ToUkmMetricHash(),
       static_cast<int64_t>(kUnblockedSurface)}};
  ukm::mojom::UkmEntryPtr ukm_entry(
      std::in_place, 1, ukm::builders::Identifiability::kEntryNameHash,
      metrics);

  ASSERT_EQ(2u, ukm_entry->metrics.size());
  base::flat_set<uint64_t> filtered;
  EXPECT_TRUE(filter->FilterEntry(ukm_entry.get(), &filtered));
  EXPECT_TRUE(filtered.empty());
  EXPECT_THAT(ukm_entry->metrics, IsSupersetOf(expected_metrics));
}

TEST(PrivacyBudgetUkmEntryFilterStandaloneTest, AddsStudyMetadataToFirstEvent) {
  // Verifies that the study metadata is included in the first event that's
  // reported.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kIdentifiabilityStudyMetaExperiment);
  TestingPrefServiceSimple pref_service;
  prefs::RegisterPrivacyBudgetPrefs(pref_service.registry());
  test::ScopedPrivacyBudgetConfig scoped_config(
      test::ScopedPrivacyBudgetConfig::Presets::kEnableRandomSampling);

  auto state =
      std::make_unique<test_utils::InspectableIdentifiabilityStudyState>(
          &pref_service);
  state->SelectAllOffsetsForTesting();
  auto filter = std::make_unique<PrivacyBudgetUkmEntryFilter>(state.get());

  base::flat_map<uint64_t, int64_t> events = {{1, 1}, {2, 2}};
  ukm::mojom::UkmEntryPtr first_entry(
      std::in_place, 1, ukm::builders::Identifiability::kEntryNameHash, events);
  ukm::mojom::UkmEntryPtr second_entry = first_entry.Clone();

  base::flat_set<uint64_t> removed_hashes;
  ASSERT_TRUE(filter->FilterEntry(first_entry.get(), &removed_hashes));

  // There should at least 4 metrics. The two in `events`, and the two
  // "metadata" metrics. In practice there will be more, e.g. `kMeasuredSurface`
  // metrics.
  EXPECT_LT(4u, first_entry->metrics.size());

  EXPECT_THAT(
      first_entry->metrics,
      IsSupersetOf(
          {Key(ukm::builders::Identifiability::kStudyGeneration_626NameHash),
           Key(ukm::builders::Identifiability::
                   kGeneratorVersion_926NameHash)}));

  ASSERT_TRUE(filter->FilterEntry(second_entry.get(), &removed_hashes));

  // This time only the metrics in `entry` should be included.
  EXPECT_EQ(2u, second_entry->metrics.size());
  EXPECT_THAT(second_entry->metrics, UnorderedElementsAre(Key(1), Key(2)));
}

TEST(PrivacyBudgetUkmEntryFilterStandaloneTest, MetaExperimentActive) {
  TestingPrefServiceSimple pref_service;
  prefs::RegisterPrivacyBudgetPrefs(pref_service.registry());
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kIdentifiabilityStudyMetaExperiment,
      {{features::kIdentifiabilityStudyMetaExperimentActivationProbability.name,
        "1"}});
  auto state =
      std::make_unique<test_utils::InspectableIdentifiabilityStudyState>(
          &pref_service);
  auto filter = std::make_unique<PrivacyBudgetUkmEntryFilter>(state.get());

  // The filter should reject all Identifiability events but report the
  // corresponding meta surfaces. The surface of type 0 (kReservedInternal)
  // should not be dropped though.
  base::flat_map<uint64_t, int64_t> events = {{0, 5}, {1, 10}, {2, 20}};
  ukm::mojom::UkmEntryPtr entry(
      std::in_place, 1, ukm::builders::Identifiability::kEntryNameHash, events);

  base::flat_set<uint64_t> filtered;
  EXPECT_TRUE(filter->FilterEntry(entry.get(), &filtered));
  EXPECT_THAT(
      entry->metrics,
      UnorderedElementsAre(
          Pair(0, 5),
          Pair(Type(blink::IdentifiableSurface::Type::kMeasuredSurface), 1),
          Pair(Type(blink::IdentifiableSurface::Type::kMeasuredSurface), 2),
          Key(ukm::builders::Identifiability::kStudyGeneration_626NameHash),
          Key(ukm::builders::Identifiability::kGeneratorVersion_926NameHash)));
}

}  // namespace
