// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/welcome_tour/welcome_tour_metrics.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/user_education/user_education_ash_test_base.h"
#include "base/containers/enum_set.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::welcome_tour_metrics {
namespace {

// Helpers ---------------------------------------------------------------------

// Clears the pref with the given `pref_name`. Must be called when there is an
// active session.
void ClearPref(const std::string& pref_name) {
  Shell::Get()->session_controller()->GetLastActiveUserPrefService()->ClearPref(
      pref_name);
}

}  // namespace

// WelcomeTourInteractionMetricsTest -------------------------------------------

// Base class for tests that verify Welcome Tour Interaction metrics are
// properly submitted.
class WelcomeTourInteractionMetricsTest
    : public UserEducationAshTestBase,
      public ::testing::WithParamInterface<absl::optional<PreventedReason>> {
 public:
  WelcomeTourInteractionMetricsTest() {
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        features::kWelcomeTour,
        {{"is-counterfactual", IsCounterfactual() ? "true" : "false"}});
  }

  std::string GetCompletionString() const {
    if (IsCounterfactual()) {
      return "Counterfactual";
    }
    if (IsCompleted()) {
      return "Completed";
    }
    NOTREACHED_NORETURN();
  }

  std::string GetInteractionCountMetricName(
      const std::string& completion_string) const {
    return base::StrCat(
        {"Ash.WelcomeTour.", completion_string, ".Interaction.Count"});
  }

  std::string GetInteractionFirstTimeMetricName(
      Interaction interaction,
      const std::string& completion_string) const {
    return base::StrCat({"Ash.WelcomeTour.", completion_string,
                         ".Interaction.FirstTime.", ToString(interaction)});
  }

  absl::optional<PreventedReason> GetPreventedReason() const {
    return GetParam();
  }

  bool IsCompleted() const { return !GetParam().has_value(); }

  bool IsCounterfactual() const {
    return GetParam() == PreventedReason::kCounterfactualExperimentArm;
  }

  bool InteractionsShouldBeRecorded() const {
    return IsCompleted() || IsCounterfactual();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    WelcomeTourInteractionMetricsTest,
    ::testing::Values(
        absl::nullopt,
        absl::make_optional(PreventedReason::kCounterfactualExperimentArm),
        absl::make_optional(PreventedReason::kUnknown)));

// Tests -----------------------------------------------------------------------

// Verifies that, when an `Interaction` is recorded for the first time, the
// appropriate histogram is submitted.
TEST_P(WelcomeTourInteractionMetricsTest, RecordInteraction) {
  SimulateNewUserFirstLogin("user@test");
  ClearPref("ash.welcome_tour.prevented.first_reason");
  ClearPref("ash.welcome_tour.prevented.first_time");

  base::HistogramTester histogram_tester;

  // Case: Before tour prevention/completion. No interactions should be logged.
  for (auto interaction : kAllInteractionsSet) {
    RecordInteraction(interaction);
    histogram_tester.ExpectTotalCount(
        GetInteractionFirstTimeMetricName(interaction, "Completed"), 0);
    histogram_tester.ExpectBucketCount(
        GetInteractionCountMetricName("Completed"), interaction, 0);
    histogram_tester.ExpectTotalCount(
        GetInteractionFirstTimeMetricName(interaction, "Counterfactual"), 0);
    histogram_tester.ExpectBucketCount(
        GetInteractionCountMetricName("Counterfactual"), interaction, 0);
  }

  // Case: First time after prevention/completion. Interactions should be
  // recorded, along with first interaction times, if the tour was completed or
  // prevented counterfactually.
  if (IsCompleted()) {
    RecordTourDuration(base::Minutes(1), /*completed=*/true);
  } else {
    RecordTourPrevented(GetPreventedReason().value());
  }

  for (auto interaction : kAllInteractionsSet) {
    RecordInteraction(interaction);
    if (InteractionsShouldBeRecorded()) {
      const auto completion = GetCompletionString();
      histogram_tester.ExpectTotalCount(
          GetInteractionFirstTimeMetricName(interaction, completion), 1);
      histogram_tester.ExpectBucketCount(
          GetInteractionCountMetricName(completion), interaction, 1);
    } else {
      histogram_tester.ExpectTotalCount(
          GetInteractionFirstTimeMetricName(interaction, "Completed"), 0);
      histogram_tester.ExpectBucketCount(
          GetInteractionCountMetricName("Completed"), interaction, 0);
      histogram_tester.ExpectTotalCount(
          GetInteractionFirstTimeMetricName(interaction, "Counterfactual"), 0);
      histogram_tester.ExpectBucketCount(
          GetInteractionCountMetricName("Counterfactual"), interaction, 0);
    }
  }

  // Case: Another time after prevention/completion. Interactions should be
  // recorded if the tour was completed or prevented counterfactually, but the
  // first time metric should not be recorded again.
  for (auto interaction : kAllInteractionsSet) {
    RecordInteraction(interaction);
    if (InteractionsShouldBeRecorded()) {
      const auto completion = GetCompletionString();
      histogram_tester.ExpectTotalCount(
          GetInteractionFirstTimeMetricName(interaction, completion), 1);
      histogram_tester.ExpectBucketCount(
          GetInteractionCountMetricName(completion), interaction, 2);
    } else {
      histogram_tester.ExpectTotalCount(
          GetInteractionFirstTimeMetricName(interaction, "Completed"), 0);
      histogram_tester.ExpectBucketCount(
          GetInteractionCountMetricName("Completed"), interaction, 0);
      histogram_tester.ExpectTotalCount(
          GetInteractionFirstTimeMetricName(interaction, "Counterfactual"), 0);
      histogram_tester.ExpectBucketCount(
          GetInteractionCountMetricName("Counterfactual"), interaction, 0);
    }
  }
}

// WelcomeTourMetricsEnumTest --------------------------------------------------

// Base class of tests that verify all valid enum values and no other are
// included in the relevant `base::EnumSet`s.
using WelcomeTourMetricsEnumTest = testing::Test;

// Tests -----------------------------------------------------------------------

TEST_F(WelcomeTourMetricsEnumTest, AllInteractions) {
  // If a value in `Interactions` is added or deprecated, the below switch
  // statement must be modified accordingly. It should be a canonical list of
  // what values are considered valid.
  for (auto interaction : base::EnumSet<Interaction, Interaction::kMinValue,
                                        Interaction::kMaxValue>::All()) {
    bool should_exist_in_all_set = false;

    switch (interaction) {
      case Interaction::kFilesApp:
      case Interaction::kLauncher:
      case Interaction::kQuickSettings:
      case Interaction::kSearch:
      case Interaction::kSettingsApp:
        should_exist_in_all_set = true;
    }

    EXPECT_EQ(kAllInteractionsSet.Has(interaction), should_exist_in_all_set);
  }
}

TEST_F(WelcomeTourMetricsEnumTest, AllPreventedReasons) {
  // If a value in `PreventedReason` is added or deprecated, the below switch
  // statement must be modified accordingly. It should be a canonical list of
  // what values are considered valid.
  for (auto reason : base::EnumSet<PreventedReason, PreventedReason::kMinValue,
                                   PreventedReason::kMaxValue>::All()) {
    bool should_exist_in_all_set = false;

    switch (reason) {
      case PreventedReason::kUnknown:
      case PreventedReason::kChromeVoxEnabled:
      case PreventedReason::kCounterfactualExperimentArm:
      case PreventedReason::kManagedAccount:
      case PreventedReason::kTabletModeEnabled:
      case PreventedReason::kUserNewnessNotAvailable:
      case PreventedReason::kUserNotNewCrossDevice:
      case PreventedReason::kUserTypeNotRegular:
      case PreventedReason::kUserNotNewLocally:
        should_exist_in_all_set = true;
    }

    EXPECT_EQ(kAllPreventedReasonsSet.Has(reason), should_exist_in_all_set);
  }
}

// WelcomeTourMetricsTest ------------------------------------------------------

// Base class for tests that verify Welcome Tour metrics are properly submitted.
class WelcomeTourMetricsTest : public testing::Test {
 public:
  WelcomeTourMetricsTest() : scoped_feature_list_(features::kWelcomeTour) {}

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests -----------------------------------------------------------------------

TEST_F(WelcomeTourMetricsTest, RecordStepAborted) {
  static constexpr char kStepAbortedMetricName[] =
      "Ash.WelcomeTour.Step.Aborted";

  for (auto step :
       base::EnumSet<Step, Step::kMinValue, Step::kMaxValue>::All()) {
    base::HistogramTester histogram_tester;

    histogram_tester.ExpectTotalCount(kStepAbortedMetricName, 0);
    RecordStepAborted(step);
    histogram_tester.ExpectBucketCount(kStepAbortedMetricName, step, 1);
    histogram_tester.ExpectTotalCount(kStepAbortedMetricName, 1);
  }
}

TEST_F(WelcomeTourMetricsTest, RecordStepDuration) {
  base::HistogramTester histogram_tester;
  for (auto step :
       base::EnumSet<Step, Step::kMinValue, Step::kMaxValue>::All()) {
    const auto step_duration_metric_name =
        base::StrCat({"Ash.WelcomeTour.Step.Duration.", ToString(step)});

    const auto test_step_length = base::Seconds(10);
    histogram_tester.ExpectTotalCount(step_duration_metric_name, 0);
    histogram_tester.ExpectTimeBucketCount(step_duration_metric_name,
                                           test_step_length, 0);
    RecordStepDuration(step, test_step_length);
    histogram_tester.ExpectTotalCount(step_duration_metric_name, 1);
    histogram_tester.ExpectTimeBucketCount(step_duration_metric_name,
                                           test_step_length, 1);
  }
}

TEST_F(WelcomeTourMetricsTest, RecordStepShown) {
  static constexpr char kStepShownMetricName[] = "Ash.WelcomeTour.Step.Shown";

  for (auto step :
       base::EnumSet<Step, Step::kMinValue, Step::kMaxValue>::All()) {
    base::HistogramTester histogram_tester;

    histogram_tester.ExpectTotalCount(kStepShownMetricName, 0);
    RecordStepShown(step);
    histogram_tester.ExpectBucketCount(kStepShownMetricName, step, 1);
    histogram_tester.ExpectTotalCount(kStepShownMetricName, 1);
  }
}

}  // namespace ash::welcome_tour_metrics
