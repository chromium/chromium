// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/holding_space_wallpaper_nudge/holding_space_wallpaper_nudge_metrics.h"

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/user_education/holding_space_wallpaper_nudge/holding_space_wallpaper_nudge_prefs.h"
#include "ash/user_education/user_education_ash_test_base.h"
#include "ash/user_education/user_education_types.h"
#include "base/containers/enum_set.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::holding_space_wallpaper_nudge_metrics {

namespace {

// Helpers ---------------------------------------------------------------------

std::string GetInteractionFirstTimeMetricName(
    Interaction interaction,
    const std::string& experiment_arm_string) {
  return base::StrCat({"Ash.HoldingSpaceWallpaperNudge.", experiment_arm_string,
                       ".Interaction.FirstTime.", ToString(interaction)});
}

PrefService* GetLastActiveUserPrefService() {
  return Shell::Get()->session_controller()->GetLastActiveUserPrefService();
}

}  // namespace

// HoldingSpaceWallpaperNudgeMetricsEnumTest -----------------------------------

// Base class of tests that verify all valid enum values and no other are
// included in the relevant `base::EnumSet`s.
using HoldingSpaceWallpaperNudgeMetricsEnumTest = testing::Test;

// Tests -----------------------------------------------------------------------

TEST_F(HoldingSpaceWallpaperNudgeMetricsEnumTest, AllInteractions) {
  // If a value in `Interactions` is added or deprecated, the below switch
  // statement must be modified accordingly. It should be a canonical list of
  // what values are considered valid.
  for (auto interaction : base::EnumSet<Interaction, Interaction::kMinValue,
                                        Interaction::kMaxValue>::All()) {
    bool should_exist_in_all_set = false;

    switch (interaction) {
      case Interaction::kDroppedFileOnHoldingSpace:
      case Interaction::kDroppedFileOnWallpaper:
      case Interaction::kDraggedFileOverWallpaper:
      case Interaction::kOpenedHoldingSpace:
      case Interaction::kPinnedFileFromAnySource:
      case Interaction::kPinnedFileFromContextMenu:
      case Interaction::kPinnedFileFromFilesApp:
      case Interaction::kPinnedFileFromHoldingSpaceDrop:
      case Interaction::kPinnedFileFromPinButton:
      case Interaction::kPinnedFileFromWallpaperDrop:
      case Interaction::kUsedOtherItem:
      case Interaction::kUsedPinnedItem:
        should_exist_in_all_set = true;
    }

    EXPECT_EQ(kAllInteractionsSet.Has(interaction), should_exist_in_all_set);
  }
}

// HoldingSpaceWallpaperNudgeMetricsTest ---------------------------------------

// Base class for tests that verify Holding Space wallpaper nudge metrics are
// properly submitted.
class HoldingSpaceWallpaperNudgeMetricsTest
    : public UserEducationAshTestBase,
      public testing::WithParamInterface<std::tuple<
          /*is-counterfactual=*/bool,
          /*is-drop-to-pin-enabled=*/bool>> {
 public:
  HoldingSpaceWallpaperNudgeMetricsTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/{{features::kHoldingSpaceWallpaperNudge,
                               {{"drop-to-pin",
                                 IsDropToPinEnabled() ? "true" : "false"},
                                {"is-counterfactual",
                                 IsCounterfactual() ? "true" : "false"}}}},
        /*disabled_features=*/{});
  }

  std::string GetExperimentArmString() const {
    if (IsCounterfactual()) {
      return "Counterfactual";
    }

    if (IsDropToPinEnabled()) {
      return "WithDropToPin";
    }

    return "WithoutDropToPin";
  }

  bool IsCounterfactual() const { return std::get<0>(GetParam()); }

  bool IsDropToPinEnabled() const { return std::get<1>(GetParam()); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    HoldingSpaceWallpaperNudgeMetricsTest,
    testing::Combine(/*is-counterfactual=*/testing::Bool(),
                     /*is-drop-to-pin-enabled=*/testing::Bool()));

// Tests -----------------------------------------------------------------------

// Confirms that `RecordFirstPin()` submits the proper metrics.
TEST_P(HoldingSpaceWallpaperNudgeMetricsTest, RecordFirstPin) {
  // Cache metric name.
  const auto metric_name =
      base::StrCat({"Ash.HoldingSpaceWallpaperNudge.", GetExperimentArmString(),
                    ".ShownBeforeFirstPin"});

  // Login and get the prefs service since these metrics depend on nudge prefs.
  SimulateNewUserFirstLogin("user@test");
  auto* prefs = GetLastActiveUserPrefService();

  base::HistogramTester histogram_tester;

  for (size_t i = 1u; i < 4u; ++i) {
    holding_space_wallpaper_nudge_prefs::MarkNudgeShown(prefs);

    RecordFirstPin();
    histogram_tester.ExpectTotalCount(metric_name, i);
    histogram_tester.ExpectBucketCount(metric_name, i, 1u);
  }
}

// Confirms that `RecordInteraction()` submits the proper metrics.
TEST_P(HoldingSpaceWallpaperNudgeMetricsTest, RecordInteraction) {
  // Cache metric name.
  auto count_metric_name =
      base::StrCat({"Ash.HoldingSpaceWallpaperNudge.", GetExperimentArmString(),
                    ".Interaction.Count"});

  // Login and get the prefs service since these metrics depend on nudge prefs.
  SimulateNewUserFirstLogin("user@test");
  auto* prefs = GetLastActiveUserPrefService();

  base::HistogramTester histogram_tester;
  size_t total_count_metrics_emitted = 0u;

  // Expect no metrics to be emitted before the time of the first eligible
  // session has been marked.
  for (auto interaction : kAllInteractionsSet) {
    RecordInteraction(interaction);

    // FirstTime metrics.
    histogram_tester.ExpectTotalCount(
        GetInteractionFirstTimeMetricName(interaction,
                                          GetExperimentArmString()),
        0u);

    // Count metrics.
    histogram_tester.ExpectBucketCount(count_metric_name, interaction, 0u);
    histogram_tester.ExpectTotalCount(count_metric_name, 0u);
  }

  // Mark the user as eligible so that metrics have a point to measure from.
  EXPECT_TRUE(
      holding_space_wallpaper_nudge_prefs::MarkTimeOfFirstEligibleSession(
          prefs));

  // Expect the FirstTime and Count metrics to both be emitted on first call.
  for (auto interaction : kAllInteractionsSet) {
    RecordInteraction(interaction);
    ++total_count_metrics_emitted;

    // FirstTime metrics.
    histogram_tester.ExpectTotalCount(
        GetInteractionFirstTimeMetricName(interaction,
                                          GetExperimentArmString()),
        1u);

    // Count metrics.
    histogram_tester.ExpectBucketCount(count_metric_name, interaction, 1u);
    histogram_tester.ExpectTotalCount(count_metric_name,
                                      total_count_metrics_emitted);
  }

  // Expect only the Count metrics to be emitted on future calls.
  for (auto interaction : kAllInteractionsSet) {
    RecordInteraction(interaction);
    ++total_count_metrics_emitted;

    // FirstTime metrics.
    histogram_tester.ExpectTotalCount(
        GetInteractionFirstTimeMetricName(interaction,
                                          GetExperimentArmString()),
        1u);

    // Count metrics.
    histogram_tester.ExpectBucketCount(count_metric_name, interaction, 2u);
    histogram_tester.ExpectTotalCount(count_metric_name,
                                      total_count_metrics_emitted);
  }
}

// Confirms that `RecordNudgeDuration()` submits the proper metrics.
TEST_P(HoldingSpaceWallpaperNudgeMetricsTest, RecordNudgeDuration) {
  // Cache metric name.
  const auto metric_name =
      base::StrCat({"Ash.HoldingSpaceWallpaperNudge.", GetExperimentArmString(),
                    ".Duration"});

  base::HistogramTester histogram_tester;

  // Expect the duration metrics to be emitted.
  const auto time_delta = base::Seconds(5);
  RecordNudgeDuration(time_delta);
  histogram_tester.ExpectTotalCount(metric_name, 1u);
  histogram_tester.ExpectTimeBucketCount(metric_name, time_delta, 1u);
}

// Confirms that `RecordNudgeShown()` submits the proper metrics.
TEST_P(HoldingSpaceWallpaperNudgeMetricsTest, RecordNudgeShown) {
  // Cache metric name.
  const auto metric_name = base::StrCat(
      {"Ash.HoldingSpaceWallpaperNudge.", GetExperimentArmString(), ".Shown"});

  // Login and get the prefs service since these metrics depend on nudge prefs.
  SimulateNewUserFirstLogin("user@test");
  auto* prefs = GetLastActiveUserPrefService();

  base::HistogramTester histogram_tester;

  for (size_t i = 1u; i < 4u; ++i) {
    holding_space_wallpaper_nudge_prefs::MarkNudgeShown(prefs);

    RecordNudgeShown();
    histogram_tester.ExpectTotalCount(metric_name, i);
    histogram_tester.ExpectBucketCount(metric_name, i, 1u);
  }
}

// Confirms that `RecordNudgeSuppressed()` submits the proper metrics.
TEST_P(HoldingSpaceWallpaperNudgeMetricsTest, RecordNudgeSuppressed) {
  using AllSuppressedReasonsSet =
      base::EnumSet<SuppressedReason, SuppressedReason::kMinValue,
                    SuppressedReason::kMaxValue>;

  // Cache metric name.
  auto metric_name =
      base::StrCat({"Ash.HoldingSpaceWallpaperNudge.", GetExperimentArmString(),
                    ".SuppressedReason"});

  base::HistogramTester histogram_tester;

  // Expect each call to emit the metric matching its `reason`.
  size_t total_count = 0u;
  for (auto reason : AllSuppressedReasonsSet::All()) {
    RecordNudgeSuppressed(reason);
    histogram_tester.ExpectTotalCount(metric_name, ++total_count);
    histogram_tester.ExpectBucketCount(metric_name, reason, 1u);
  }
}

// Confirms that `RecordUserEligibility()` submits the proper metrics.
TEST_P(HoldingSpaceWallpaperNudgeMetricsTest, RecordUserEligibility) {
  using AllIneligibleReasonsSet =
      base::EnumSet<IneligibleReason, IneligibleReason::kMinValue,
                    IneligibleReason::kMaxValue>;

  // Cache metric names.
  const auto eligible_metric_name =
      base::StrCat({"Ash.HoldingSpaceWallpaperNudge.", GetExperimentArmString(),
                    ".Eligible"});
  const auto reason_metric_name =
      base::StrCat({"Ash.HoldingSpaceWallpaperNudge.", GetExperimentArmString(),
                    ".IneligibleReason"});

  // Track the total number of eligibility metrics and ineligible reason metrics
  // that should have been submitted.
  size_t total_eligibility_count = 0u;
  size_t total_reason_count = 0u;

  base::HistogramTester histogram_tester;

  // Recording with no `IneligibleReason` should log metrics indicating the user
  // is eligible.
  RecordUserEligibility(std::nullopt);
  ++total_eligibility_count;
  histogram_tester.ExpectTotalCount(eligible_metric_name,
                                    total_eligibility_count);
  histogram_tester.ExpectBucketCount(eligible_metric_name, true, 1u);

  // Recording with an `IneligibleReason` given should log metrics indicating
  // the user is ineligible and metrics containing that reason.
  for (auto reason : AllIneligibleReasonsSet::All()) {
    RecordUserEligibility(reason);
    ++total_reason_count;
    ++total_eligibility_count;

    // Pure eligibility metrics.
    histogram_tester.ExpectTotalCount(eligible_metric_name,
                                      total_eligibility_count);
    histogram_tester.ExpectBucketCount(eligible_metric_name, false,
                                       total_reason_count);

    // `IneligibleReason` reason metrics.
    histogram_tester.ExpectTotalCount(reason_metric_name, total_reason_count);
    histogram_tester.ExpectBucketCount(reason_metric_name, reason, 1u);
  }
}

}  // namespace ash::holding_space_wallpaper_nudge_metrics
