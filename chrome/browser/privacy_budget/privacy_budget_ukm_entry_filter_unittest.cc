// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_budget/privacy_budget_ukm_entry_filter.h"

#include <cstdint>

#include "base/containers/flat_map.h"
#include "base/template_util.h"
#include "chrome/browser/privacy_budget/identifiability_study_state.h"
#include "chrome/common/privacy_budget/scoped_privacy_budget_config.h"
#include "components/prefs/testing_pref_service.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/mojom/ukm_interface.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"

namespace {

class PrivacyBudgetUkmEntryFilterTest : public ::testing::Test {
 public:
  PrivacyBudgetUkmEntryFilterTest() {
    // Uses FeatureLists. Hence needs to be initialized in the constructor lest
    // we add any multithreading tests here.
    auto parameters = test::ScopedPrivacyBudgetConfig::Parameters{};
    config_.Apply(parameters);
    prefs::RegisterPrivacyBudgetPrefs(pref_service_.registry());
  }

  TestingPrefServiceSimple* pref_service() { return &pref_service_; }

 private:
  TestingPrefServiceSimple pref_service_;
  test::ScopedPrivacyBudgetConfig config_;
};

}  // namespace

TEST(PrivacyBudgetUkmEntryFilterStandaloneTest,
     BlocksIdentifiabilityMetricsByDefault) {
  IdentifiabilityStudyState::ResetStateForTesting();
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
  IdentifiabilityStudyState::ResetStateForTesting();
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
  EXPECT_EQ(expected_metrics, x->metrics);
}
