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

// Constants -------------------------------------------------------------------

static constexpr auto kAllStepsSet =
    base::EnumSet<Step, Step::kMinValue, Step::kMaxValue>::All();

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
      public ::testing::WithParamInterface<std::optional<PreventedReason>> {
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

  std::optional<PreventedReason> GetPreventedReason() const {
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
        std::nullopt,
        std::make_optional(PreventedReason::kCounterfactualExperimentArm),
        std::make_optional(PreventedReason::kUnknown)));

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

// Verifies that attempting to record an interaction before login doesn't crash.
TEST_P(WelcomeTourInteractionMetricsTest, RecordInteractionBeforeLogin) {
  EXPECT_FALSE(
      Shell::Get()->session_controller()->GetLastActiveUserPrefService());
  for (auto interaction : kAllInteractionsSet) {
    RecordInteraction(interaction);
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
      case Interaction::kExploreApp:
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
class WelcomeTourMetricsTest : public UserEducationAshTestBase {
 protected:
  // Verifies that `record_function` will successfully record all enum values in
  // `valid_enum_set` to a histogram with name `metric_name`.
  template <typename E>
  static void TestEnumHistogram(
      const std::string& metric_name,
      base::EnumSet<E, E::kMinValue, E::kMaxValue> valid_enum_set,
      base::FunctionRef<void(E)> record_function) {
    static_assert(std::is_enum<E>::value);

    for (auto value : valid_enum_set) {
      base::HistogramTester histogram_tester;

      record_function(value);
      histogram_tester.ExpectBucketCount(metric_name, value, 1);
      histogram_tester.ExpectTotalCount(metric_name, 1);
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{features::kWelcomeTour};
};

// Tests -----------------------------------------------------------------------

// Verifies that all valid values of the `Step` enum can be successfully
// recorded by the `RecordStepAborted()` utility function.
TEST_F(WelcomeTourMetricsTest, RecordStepAborted) {
  TestEnumHistogram<Step>("Ash.WelcomeTour.Step.Aborted", kAllStepsSet,
                          &RecordStepAborted);
}

// Verifies that all valid values of the `Step` enum record their associated
// duration metrics through the `RecordStepDuration()` utility function.
TEST_F(WelcomeTourMetricsTest, RecordStepDuration) {
  base::HistogramTester histogram_tester;
  for (auto step : kAllStepsSet) {
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

// Verifies that all valid values of the `Step` enum can be successfully
// recorded by the `RecordStepShown()` utility function.
TEST_F(WelcomeTourMetricsTest, RecordStepShown) {
  TestEnumHistogram<Step>("Ash.WelcomeTour.Step.Shown", kAllStepsSet,
                          &RecordStepShown);
}

// Verifies that all valid values of the `AbortedReason` enum can be
// successfully recorded by the `RecordTourAborted()` utility function.
TEST_F(WelcomeTourMetricsTest, RecordTourAborted) {
  using AbortedReasonSetType =
      base::EnumSet<AbortedReason, AbortedReason::kMinValue,
                    AbortedReason::kMaxValue>;

  TestEnumHistogram<AbortedReason>("Ash.WelcomeTour.Aborted.Reason",
                                   AbortedReasonSetType::All(),
                                   &RecordTourAborted);
}

TEST_F(WelcomeTourMetricsTest, RecordTourDuration) {
  static constexpr char kAbortedTourDurationMetricName[] =
      "Ash.WelcomeTour.Aborted.Duration";
  static constexpr char kCompletedTourDurationMetricName[] =
      "Ash.WelcomeTour.Completed.Duration";
  static constexpr auto kTestTourLength = base::Seconds(30);

  SimulateNewUserFirstLogin("user@test");

  // Case: Tour is aborted.
  {
    base::HistogramTester histogram_tester;

    RecordTourDuration(kTestTourLength, /*completed=*/false);
    histogram_tester.ExpectTotalCount(kAbortedTourDurationMetricName, 1);
    histogram_tester.ExpectTotalCount(kCompletedTourDurationMetricName, 0);
    histogram_tester.ExpectTimeBucketCount(kAbortedTourDurationMetricName,
                                           kTestTourLength, 1);
  }

  // Case: Tour is completed.
  {
    base::HistogramTester histogram_tester;

    RecordTourDuration(kTestTourLength, /*completed=*/true);
    histogram_tester.ExpectTotalCount(kAbortedTourDurationMetricName, 0);
    histogram_tester.ExpectTotalCount(kCompletedTourDurationMetricName, 1);
    histogram_tester.ExpectTimeBucketCount(kCompletedTourDurationMetricName,
                                           kTestTourLength, 1);
  }
}

// Verifies that all valid values of the `PreventedReason` enum can be
// successfully recorded by the `RecordTourPrevented()` utility function.
TEST_F(WelcomeTourMetricsTest, RecordTourPrevented) {
  SimulateNewUserFirstLogin("user@test");
  TestEnumHistogram<PreventedReason>("Ash.WelcomeTour.Prevented.Reason",
                                     kAllPreventedReasonsSet,
                                     &RecordTourPrevented);
}

}  // namespace ash::welcome_tour_metrics
