// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_budget/privacy_budget_ukm_entry_filter.h"

#include <cstdint>

#include "base/containers/flat_map.h"
#include "base/metrics/metrics_hashes.h"
#include "base/strings/stringprintf.h"
#include "base/template_util.h"
#include "chrome/browser/privacy_budget/identifiability_study_state.h"
#include "chrome/common/privacy_budget/scoped_privacy_budget_config.h"
#include "components/prefs/testing_pref_service.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/mojom/ukm_interface.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"

using testing::IsSupersetOf;
using testing::Key;
using testing::UnorderedElementsAre;

TEST(PrivacyBudgetUkmEntryFilterStandaloneTest,
     BlocksIdentifiabilityMetricsByDefault) {
  IdentifiabilityStudyState::ResetGlobalStudySettingsForTesting();
  TestingPrefServiceSimple pref_service;
  prefs::RegisterPrivacyBudgetPrefs(pref_service.registry());
  auto settings = std::make_unique<IdentifiabilityStudyState>(&pref_service);
  auto filter = std::make_unique<PrivacyBudgetUkmEntryFilter>(settings.get());

  // By default the filter should reject all Identifiability events:
  base::flat_map<uint64_t, int64_t> events = {{1, 1}, {2, 2}};
  ukm::mojom::UkmEntryPtr x(base::in_place, 1,
                            ukm::builders::Identifiability::kEntryNameHash,
                            events);

  base::flat_set<uint64_t> filtered;
  EXPECT_FALSE(filter->FilterEntry(x.get(), &filtered));
  EXPECT_TRUE(filtered.empty());
}

TEST(PrivacyBudgetUkmEntryFilterStandaloneTest, AllowsOtherMetricsByDefault) {
  IdentifiabilityStudyState::ResetGlobalStudySettingsForTesting();
  TestingPrefServiceSimple pref_service;
  prefs::RegisterPrivacyBudgetPrefs(pref_service.registry());
  auto settings = std::make_unique<IdentifiabilityStudyState>(&pref_service);
  auto filter = std::make_unique<PrivacyBudgetUkmEntryFilter>(settings.get());

  base::flat_map<uint64_t, int64_t> events = {{1, 1}, {2, 2}};
  ukm::mojom::UkmEntryPtr x(base::in_place, 1,
                            ukm::builders::Blink_UseCounter::kEntryNameHash,
                            events);

  base::flat_set<uint64_t> filtered;
  EXPECT_TRUE(filter->FilterEntry(x.get(), &filtered));
  EXPECT_TRUE(filtered.empty());
  EXPECT_EQ(2u, x->metrics.size());
}

TEST(PrivacyBudgetUkmEntryFilterStandaloneTest, BlockListedMetrics) {
  IdentifiabilityStudyState::ResetGlobalStudySettingsForTesting();
  constexpr uint64_t kBlockedSurface = 1;
  constexpr uint64_t kUnblockedSurface = 2;

  test::ScopedPrivacyBudgetConfig::Parameters parameters;
  parameters.blocked_surfaces.push_back(
      blink::IdentifiableSurface::FromMetricHash(kBlockedSurface));
  test::ScopedPrivacyBudgetConfig scoped_config;
  scoped_config.Apply(parameters);

  TestingPrefServiceSimple pref_service;
  prefs::RegisterPrivacyBudgetPrefs(pref_service.registry());
  auto settings = std::make_unique<IdentifiabilityStudyState>(&pref_service);
  auto filter = std::make_unique<PrivacyBudgetUkmEntryFilter>(settings.get());

  base::flat_map<uint64_t, int64_t> metrics = {{kBlockedSurface, 1},
                                               {kUnblockedSurface, 2}};
  base::flat_map<uint64_t, int64_t> expected_metrics = {
      {kUnblockedSurface, 2},
      {blink::IdentifiableSurface::FromTypeAndToken(
           blink::IdentifiableSurface::Type::kMeasuredSurface, 0)
           .ToUkmMetricHash(),
       static_cast<int64_t>(kUnblockedSurface)}};
  ukm::mojom::UkmEntryPtr x(base::in_place, 1,
                            ukm::builders::Identifiability::kEntryNameHash,
                            metrics);

  ASSERT_EQ(2u, x->metrics.size());
  base::flat_set<uint64_t> filtered;
  EXPECT_TRUE(filter->FilterEntry(x.get(), &filtered));
  EXPECT_TRUE(filtered.empty());
  EXPECT_THAT(x->metrics, IsSupersetOf(expected_metrics));
}

TEST(PrivacyBudgetUkmEntryFilterStandaloneTest, AppliesMetadata) {
  IdentifiabilityStudyState::ResetGlobalStudySettingsForTesting();
  TestingPrefServiceSimple pref_service;
  prefs::RegisterPrivacyBudgetPrefs(pref_service.registry());
  test::ScopedPrivacyBudgetConfig::Parameters parameters;
  parameters.enabled = true;
  parameters.surface_selection_rate = 1;
  test::ScopedPrivacyBudgetConfig scoped_config;
  scoped_config.Apply(parameters);

  auto settings = std::make_unique<IdentifiabilityStudyState>(&pref_service);
  auto filter = std::make_unique<PrivacyBudgetUkmEntryFilter>(settings.get());

  base::flat_map<uint64_t, int64_t> events = {{1, 1}, {2, 2}};
  ukm::mojom::UkmEntryPtr first_entry(
      base::in_place, 1, ukm::builders::Identifiability::kEntryNameHash,
      events);
  ukm::mojom::UkmEntryPtr second_entry = first_entry.Clone();

  base::flat_set<uint64_t> filtered;
  ASSERT_TRUE(filter->FilterEntry(first_entry.get(), &filtered));

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

  ASSERT_TRUE(filter->FilterEntry(second_entry.get(), &filtered));

  // This time only the metrics in `entry` should be included.
  EXPECT_EQ(2u, second_entry->metrics.size());
  EXPECT_THAT(second_entry->metrics, UnorderedElementsAre(Key(1), Key(2)));
}
