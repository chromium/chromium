// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/welcome_tour/welcome_tour_metrics.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/user_education/user_education_ash_test_base.h"
#include "ash/user_education/user_education_util.h"
#include "base/containers/enum_set.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::welcome_tour_metrics {
namespace {

// Aliases ---------------------------------------------------------------------

using TestVariantsParam = std::tuple<
    /*is_completed=*/std::optional<bool>,
    std::optional<PreventedReason>>;

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

// WelcomeTourChangedExperimentalArmMetricTest ---------------------------------

// Base class for tests that verify Welcome Tour `ChangedExperimentalArm`
// metric is properly submitted.
class WelcomeTourChangedExperimentalArmMetricTest
    : public UserEducationAshTestBase,
      public ::testing::WithParamInterface<
          std::tuple</*pref_value=*/std::optional<ExperimentalArm>,
                     /*enabled_arm=*/std::optional<ExperimentalArm>>> {
 public:
  WelcomeTourChangedExperimentalArmMetricTest() {
    // These tests are not concerned with user eligibility, so explicitly force
    // user eligibility for the Welcome Tour.
    scoped_feature_list.InitWithFeatureStates(
        {{features::kWelcomeTourCounterfactualArm, IsV1Enabled()},
         {features::kWelcomeTourHoldbackArm, IsHoldbackEnabled()},
         {features::kWelcomeTourV2, IsV2Enabled()},
         {features::kWelcomeTourForceUserEligibility, true}});
  }

  std::optional<ExperimentalArm> GetPrefValue() const {
    return std::get<0>(GetParam());
  }

  std::optional<ExperimentalArm> GetEnabledArm() const {
    return std::get<1>(GetParam());
  }

  bool IsPrefValueHoldback() const {
    return GetPrefValue() == ExperimentalArm::kHoldback;
  }

  bool IsPrefValueV1() const { return GetPrefValue() == ExperimentalArm::kV1; }

  bool IsPrefValueV2() const { return GetPrefValue() == ExperimentalArm::kV2; }

  bool IsHoldbackEnabled() const {
    return GetEnabledArm() == ExperimentalArm::kHoldback;
  }

  bool IsV1Enabled() const { return GetEnabledArm() == ExperimentalArm::kV1; }

  bool IsV2Enabled() const { return GetEnabledArm() == ExperimentalArm::kV2; }

 private:
  base::test::ScopedFeatureList scoped_feature_list;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    WelcomeTourChangedExperimentalArmMetricTest,
    ::testing::Combine(
        /*pref_value=*/
        ::testing::Values(std::nullopt,
                          std::make_optional(ExperimentalArm::kHoldback),
                          std::make_optional(ExperimentalArm::kV1),
                          std::make_optional(ExperimentalArm::kV2)),
        /*enabled_arm=*/
        ::testing::Values(std::nullopt,
                          std::make_optional(ExperimentalArm::kHoldback),
                          std::make_optional(ExperimentalArm::kV1),
                          std::make_optional(ExperimentalArm::kV2))));

// Tests -----------------------------------------------------------------------

// Verifies that appropriate `ChangedExperimentalArm` histogram is recorded.
TEST_P(WelcomeTourChangedExperimentalArmMetricTest,
       RecordChangedExperimentalArm) {
  base::HistogramTester histogram_tester;

  // Add a primary user session for an existing user. This should *not* trigger
  // the Welcome Tour to start.
  const auto primary_account_id = AccountId::FromUserEmail("primary@test");
  auto* const session_controller_client = GetSessionControllerClient();
  session_controller_client->AddUserSession(
      primary_account_id.GetUserEmail(), user_manager::UserType::kRegular,
      /*provide_pref_service=*/true, /*is_new_profile=*/false);

  const std::optional<ExperimentalArm> pref_value = GetPrefValue();
  if (pref_value) {
    Shell::Get()
        ->session_controller()
        ->GetLastActiveUserPrefService()
        ->SetInteger("ash.welcome_tour.v2.experimental_arm.first",
                     static_cast<int>(pref_value.value()));
  }

  session_controller_client->SetSessionState(
      session_manager::SessionState::ACTIVE);

  // If there is change between the pref value and the enabled experimental
  // arms, the metric will be recorded.
  std::vector<base::Bucket> histogram_buckets;
  if (const auto enabled_arm = GetEnabledArm();
      enabled_arm && pref_value && enabled_arm != pref_value) {
    histogram_buckets.emplace_back(pref_value.value(), 1);
  }

  EXPECT_THAT(
      histogram_tester.GetAllSamples("Ash.WelcomeTour.ChangedExperimentalArm"),
      BucketsAre(histogram_buckets));
}

// WelcomeTourExperimentalArmMetricTest ----------------------------------------

// Base class for tests that verify Welcome Tour ExperimentalArm metric is
// properly submitted.
class WelcomeTourExperimentalArmMetricTest
    : public UserEducationAshTestBase,
      public ::testing::WithParamInterface<
          /*enabled_arm=*/std::optional<ExperimentalArm>> {
 public:
  WelcomeTourExperimentalArmMetricTest() {
    // These tests are not concerned with user eligibility, so explicitly force
    // user eligibility for the Welcome Tour.
    scoped_feature_list.InitWithFeatureStates(
        {{features::kWelcomeTourCounterfactualArm, IsV1Enabled()},
         {features::kWelcomeTourHoldbackArm, IsHoldbackEnabled()},
         {features::kWelcomeTourV2, IsV2Enabled()},
         {features::kWelcomeTourForceUserEligibility, true}});
  }

  std::optional<ExperimentalArm> GetEnabledArm() const { return GetParam(); }

  bool IsHoldbackEnabled() const {
    return GetEnabledArm() == ExperimentalArm::kHoldback;
  }

  bool IsV1Enabled() const { return GetEnabledArm() == ExperimentalArm::kV1; }

  bool IsV2Enabled() const { return GetEnabledArm() == ExperimentalArm::kV2; }

 private:
  base::test::ScopedFeatureList scoped_feature_list;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    WelcomeTourExperimentalArmMetricTest,
    /*enabled_arm=*/
    ::testing::Values(std::nullopt,
                      std::make_optional(ExperimentalArm::kHoldback),
                      std::make_optional(ExperimentalArm::kV1),
                      std::make_optional(ExperimentalArm::kV2)));

// Tests -----------------------------------------------------------------------

// Verifies that appropriate `ExperimentalArm` histogram is recorded.
TEST_P(WelcomeTourExperimentalArmMetricTest, RecordExperimentalArm) {
  base::HistogramTester histogram_tester;

  // Login the primary user for the first time and verify expectations.
  const auto primary_account_id = AccountId::FromUserEmail("primary@test");
  SimulateNewUserFirstLogin(primary_account_id.GetUserEmail());

  // Set histogram expectations.
  std::vector<base::Bucket> histogram_buckets;
  if (const auto enabled_arm = GetEnabledArm()) {
    histogram_buckets.emplace_back(enabled_arm.value(), 1);
  }

  // Verify histograms.
  EXPECT_THAT(histogram_tester.GetAllSamples("Ash.WelcomeTour.ExperimentalArm"),
              BucketsAre(histogram_buckets));

  EXPECT_THAT(
      histogram_tester.GetAllSamples("Ash.WelcomeTour.ChangedExperimentalArm"),
      ::testing::IsEmpty());
}

// WelcomeTourInteractionMetricsTest -------------------------------------------

// Base class for tests that verify Welcome Tour Interaction metrics are
// properly submitted.
class WelcomeTourInteractionMetricsTest
    : public UserEducationAshTestBase,
      public ::testing::WithParamInterface<TestVariantsParam> {
 public:
  WelcomeTourInteractionMetricsTest() {
    // Only one of those features can be enabled at a time.
    scoped_feature_list.InitWithFeatureStates(
        {{features::kWelcomeTourHoldbackArm, IsHoldback()},
         {features::kWelcomeTourV2, false},
         {features::kWelcomeTourCounterfactualArm, false}});
  }

  std::string GetInteractionCountMetricName() const {
    return "Ash.WelcomeTour.Interaction.Count";
  }

  std::string GetInteractionFirstTimeBucketMetricName(
      Interaction interaction) const {
    return base::StrCat({"Ash.WelcomeTour.Interaction.FirstTimeBucket.",
                         ToString(interaction)});
  }

  std::string GetInteractionFirstTimeMetricName(Interaction interaction) const {
    return base::StrCat(
        {"Ash.WelcomeTour.Interaction.FirstTime.", ToString(interaction)});
  }

  std::optional<PreventedReason> GetPreventedReason() const {
    return std::get<1>(GetParam());
  }

  std::optional<bool> IsCompleted() const { return std::get<0>(GetParam()); }

  bool IsHoldback() const {
    return GetPreventedReason() == PreventedReason::kHoldbackExperimentArm;
  }

  bool InteractionsShouldBeRecorded() const {
    return IsCompleted().has_value() || IsHoldback();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    WelcomeTourInteractionMetricsTest,
    testing::Combine(
        /*is_completed=*/::testing::Values(std::nullopt,
                                           std::make_optional(true),
                                           std::make_optional(false)),
        ::testing::Values(
            std::nullopt,
            std::make_optional(PreventedReason::kHoldbackExperimentArm),
            std::make_optional(PreventedReason::kUnknown))));

// Tests -----------------------------------------------------------------------

// Verifies that, when an `Interaction` is recorded for the first time, the
// appropriate histogram is submitted.
TEST_P(WelcomeTourInteractionMetricsTest, RecordInteraction) {
  SimulateNewUserFirstLogin("user@test");
  ClearPref("ash.welcome_tour.v2.prevented.first_reason");
  ClearPref("ash.welcome_tour.v2.prevented.first_time");

  base::HistogramTester histogram_tester;
  PrefService* prefs = user_education_util::GetLastActiveUserPrefService();

  // Case: Before tour attempt. No interactions should be logged.
  for (auto interaction : kAllInteractionsSet) {
    RecordInteraction(prefs, interaction);
    histogram_tester.ExpectTotalCount(
        GetInteractionFirstTimeBucketMetricName(interaction), 0);
    histogram_tester.ExpectTotalCount(
        GetInteractionFirstTimeMetricName(interaction), 0);
    histogram_tester.ExpectBucketCount(GetInteractionCountMetricName(),
                                       interaction, 0);
  }

  // Case: First time after tour attempt. Interactions should be recorded, along
  // with first interaction times, if the tour was attempted.
  if (const auto completed = IsCompleted()) {
    RecordTourDuration(prefs, base::Minutes(1), completed.value());
  } else if (GetPreventedReason()) {
    RecordTourPrevented(prefs, GetPreventedReason().value());
  }

  for (auto interaction : kAllInteractionsSet) {
    RecordInteraction(prefs, interaction);

    if (InteractionsShouldBeRecorded()) {
      histogram_tester.ExpectTotalCount(
          GetInteractionFirstTimeBucketMetricName(interaction), 1);
      histogram_tester.ExpectTotalCount(
          GetInteractionFirstTimeMetricName(interaction), 1);
      histogram_tester.ExpectBucketCount(GetInteractionCountMetricName(),
                                         interaction, 1);
    } else {
      histogram_tester.ExpectTotalCount(
          GetInteractionFirstTimeBucketMetricName(interaction), 0);
      histogram_tester.ExpectTotalCount(
          GetInteractionFirstTimeMetricName(interaction), 0);
      histogram_tester.ExpectBucketCount(GetInteractionCountMetricName(),
                                         interaction, 0);
    }
  }

  // Case: Another time after tour attempt. Interactions should be recorded if
  // the tour was attempted, but the first time metric should not be recorded
  // again.
  for (auto interaction : kAllInteractionsSet) {
    RecordInteraction(prefs, interaction);

    if (InteractionsShouldBeRecorded()) {
      histogram_tester.ExpectTotalCount(
          GetInteractionFirstTimeBucketMetricName(interaction), 1);
      histogram_tester.ExpectTotalCount(
          GetInteractionFirstTimeMetricName(interaction), 1);
      histogram_tester.ExpectBucketCount(GetInteractionCountMetricName(),
                                         interaction, 2);
    } else {
      histogram_tester.ExpectTotalCount(
          GetInteractionFirstTimeBucketMetricName(interaction), 0);
      histogram_tester.ExpectTotalCount(
          GetInteractionFirstTimeMetricName(interaction), 0);
      histogram_tester.ExpectBucketCount(GetInteractionCountMetricName(),
                                         interaction, 0);
    }
  }
}

// Verifies that attempting to record an interaction before login doesn't crash.
TEST_P(WelcomeTourInteractionMetricsTest, RecordInteractionBeforeLogin) {
  PrefService* prefs = user_education_util::GetLastActiveUserPrefService();
  EXPECT_FALSE(prefs);
  for (auto interaction : kAllInteractionsSet) {
    RecordInteraction(prefs, interaction);
  }
}

// WelcomeTourMetricsEnumTest --------------------------------------------------

// Base class of tests that verify all valid enum values and no others are
// included in the relevant `base::EnumSet`s.
using WelcomeTourMetricsEnumTest = testing::Test;

// Tests -----------------------------------------------------------------------

TEST_F(WelcomeTourMetricsEnumTest, AllExperimentalArms) {
  // If a value in `ExperimentalArm` is added or deprecated, the below switch
  // statement must be modified accordingly. It should be a canonical list of
  // what values are considered valid.
  for (auto arm : base::EnumSet<ExperimentalArm, ExperimentalArm::kMinValue,
                                ExperimentalArm::kMaxValue>::All()) {
    bool should_exist_in_all_set = false;

    switch (arm) {
      case ExperimentalArm::kHoldback:
      case ExperimentalArm::kV1:
      case ExperimentalArm::kV2:
        should_exist_in_all_set = true;
    }

    EXPECT_EQ(kAllExperimentalArmsSet.Has(arm), should_exist_in_all_set);
  }
}

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
      case PreventedReason::kManagedAccount:
      case PreventedReason::kTabletModeEnabled:
      case PreventedReason::kUserNewnessNotAvailable:
      case PreventedReason::kUserNotNewCrossDevice:
      case PreventedReason::kUserTypeNotRegular:
      case PreventedReason::kUserNotNewLocally:
      case PreventedReason::kHoldbackExperimentArm:
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
  PrefService* prefs = user_education_util::GetLastActiveUserPrefService();

  // Case: Tour is aborted.
  {
    base::HistogramTester histogram_tester;

    RecordTourDuration(prefs, kTestTourLength, /*completed=*/false);
    histogram_tester.ExpectTotalCount(kAbortedTourDurationMetricName, 1);
    histogram_tester.ExpectTotalCount(kCompletedTourDurationMetricName, 0);
    histogram_tester.ExpectTimeBucketCount(kAbortedTourDurationMetricName,
                                           kTestTourLength, 1);
  }

  // Case: Tour is completed.
  {
    base::HistogramTester histogram_tester;

    RecordTourDuration(prefs, kTestTourLength, /*completed=*/true);
    histogram_tester.ExpectTotalCount(kAbortedTourDurationMetricName, 0);
    histogram_tester.ExpectTotalCount(kCompletedTourDurationMetricName, 1);
    histogram_tester.ExpectTimeBucketCount(kCompletedTourDurationMetricName,
                                           kTestTourLength, 1);
  }
}

// Verifies that all valid values of the `PreventedReason` enum can be
// successfully recorded by the `RecordTourPrevented()` utility function.
TEST_F(WelcomeTourMetricsTest, RecordTourPrevented) {
  static constexpr char kTourPreventedReasonMetricName[] =
      "Ash.WelcomeTour.Prevented.Reason";

  SimulateNewUserFirstLogin("user@test");
  PrefService* prefs = user_education_util::GetLastActiveUserPrefService();

  for (auto reason : kAllPreventedReasonsSet) {
    base::HistogramTester histogram_tester;
    RecordTourPrevented(prefs, reason);
    histogram_tester.ExpectUniqueSample(kTourPreventedReasonMetricName, reason,
                                        1);
  }
}

}  // namespace ash::welcome_tour_metrics
