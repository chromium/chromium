// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tpcd/experiment/experiment_manager_impl.h"

#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/tpcd/experiment/tpcd_experiment_features.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/testing_pref_service.h"
#include "components/privacy_sandbox/tpcd_pref_names.h"
#include "components/privacy_sandbox/tpcd_utils.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace tpcd::experiment {
namespace {

class TestingExperimentManagerImpl : public ExperimentManagerImpl {
 public:
  bool CanRegisterSyntheticTrialForTesting() const {
    return CanRegisterSyntheticTrial();
  }
};

using ::testing::InSequence;
using ::testing::Optional;

using Checkpoint = ::testing::MockFunction<void(int step)>;

}  // namespace

class ExperimentManagerImplTestBase : public testing::Test {
 public:
  PrefService& prefs() { return *profile_manager_.local_state()->Get(); }

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());

    prefs().SetInteger(
        prefs::kTPCDExperimentClientState,
        static_cast<int>(utils::ExperimentState::kUnknownEligibility));
    delay_time_ = kDecisionDelayTime.Get();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
  base::MockCallback<ExperimentManager::EligibilityDecisionCallback>
      mock_callback_;
  base::TimeDelta delay_time_;
};

TEST_F(ExperimentManagerImplTestBase, Version) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kCookieDeprecationFacilitatedTesting, {{"version", "2"}});

  const struct {
    const char* desc;
    std::optional<int> initial_version;
    std::optional<utils::ExperimentState> initial_state;
    int expected_version;
    utils::ExperimentState expected_state;
  } kTestCases[] = {
      {
          .desc = "first-run",
          .expected_version = 2,
          .expected_state = utils::ExperimentState::kUnknownEligibility,
      },
      {
          .desc = "new-version",
          .initial_version = 1,
          .initial_state = utils::ExperimentState::kEligible,
          .expected_version = 2,
          .expected_state = utils::ExperimentState::kUnknownEligibility,
      },
      {
          .desc = "same-version",
          .initial_version = 2,
          .initial_state = utils::ExperimentState::kEligible,
          .expected_version = 2,
          .expected_state = utils::ExperimentState::kEligible,
      },
      {
          .desc = "old-version",
          .initial_version = 3,
          .initial_state = utils::ExperimentState::kIneligible,
          .expected_version = 2,
          .expected_state = utils::ExperimentState::kUnknownEligibility,
      },
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.desc);

    if (test_case.initial_version.has_value()) {
      prefs().SetInteger(prefs::kTPCDExperimentClientStateVersion,
                         *test_case.initial_version);
    } else {
      prefs().ClearPref(prefs::kTPCDExperimentClientStateVersion);
    }
    if (test_case.initial_state.has_value()) {
      prefs().SetInteger(prefs::kTPCDExperimentClientState,
                         static_cast<int>(*test_case.initial_state));
    } else {
      prefs().ClearPref(prefs::kTPCDExperimentClientState);
    }

    TestingExperimentManagerImpl experiment_manager;

    EXPECT_EQ(prefs().GetInteger(prefs::kTPCDExperimentClientStateVersion),
              test_case.expected_version);
    EXPECT_EQ(prefs().GetInteger(prefs::kTPCDExperimentClientState),
              static_cast<int>(test_case.expected_state));

    EXPECT_EQ(experiment_manager.DidVersionChange(),
              test_case.initial_version != 2);
  }
}

TEST_F(ExperimentManagerImplTestBase, ForceEligibleForTesting) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kCookieDeprecationFacilitatedTesting,
      {{"force_eligible", "true"}});

  EXPECT_CALL(mock_callback_, Run(true)).Times(1);

  TestingExperimentManagerImpl test_manager;
  EXPECT_THAT(test_manager.IsClientEligible(), testing::Optional(true));

  // This should do nothing.
  test_manager.SetClientEligibility(/*is_eligible=*/false,
                                    mock_callback_.Get());

  task_environment_.FastForwardBy(delay_time_);
  EXPECT_THAT(test_manager.IsClientEligible(), testing::Optional(true));
}

class ExperimentManagerImplTest : public ExperimentManagerImplTestBase {
 public:
  ExperimentManagerImplTest() {
    feature_list_.InitAndEnableFeature(
        features::kCookieDeprecationFacilitatedTesting);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(ExperimentManagerImplTest,
       ExperimentManager_OneEligibleProfileCallSetsPrefEligible) {
  TestingExperimentManagerImpl test_manager;
  test_manager.SetClientEligibility(/*is_eligible=*/true, mock_callback_.Get());
  EXPECT_CALL(mock_callback_, Run(true)).Times(1);
  task_environment_.FastForwardBy(delay_time_);

  EXPECT_EQ(prefs().GetInteger(prefs::kTPCDExperimentClientState),
            static_cast<int>(utils::ExperimentState::kEligible));
}

TEST_F(
    ExperimentManagerImplTest,
    ExperimentManager_OneIneligibleProfileCallSetsPrefIneligibleAndReturnsEarly) {
  TestingExperimentManagerImpl test_manager;
  test_manager.SetClientEligibility(/*is_eligible=*/false,
                                    mock_callback_.Get());
  EXPECT_CALL(mock_callback_, Run(false)).Times(1);
  task_environment_.FastForwardBy(delay_time_);

  EXPECT_EQ(prefs().GetInteger(prefs::kTPCDExperimentClientState),
            static_cast<int>(utils::ExperimentState::kIneligible));
}

TEST_F(
    ExperimentManagerImplTest,
    ExperimentManager_OneEligibleOneIneligibleProfileCallSetsPrefIneligible) {
  TestingExperimentManagerImpl test_manager;
  test_manager.SetClientEligibility(/*is_eligible=*/true, mock_callback_.Get());
  test_manager.SetClientEligibility(/*is_eligible=*/false,
                                    mock_callback_.Get());
  EXPECT_CALL(mock_callback_, Run(false)).Times(2);
  task_environment_.FastForwardBy(delay_time_);

  EXPECT_EQ(prefs().GetInteger(prefs::kTPCDExperimentClientState),
            static_cast<int>(utils::ExperimentState::kIneligible));
}

TEST_F(
    ExperimentManagerImplTest,
    ExperimentManager_OneIneligibleOneEligibleProfileCallSetsPrefIneligible) {
  TestingExperimentManagerImpl test_manager;
  test_manager.SetClientEligibility(/*is_eligible=*/false,
                                    mock_callback_.Get());
  test_manager.SetClientEligibility(/*is_eligible=*/true, mock_callback_.Get());
  EXPECT_CALL(mock_callback_, Run(false)).Times(2);
  task_environment_.FastForwardBy(delay_time_);

  EXPECT_EQ(prefs().GetInteger(prefs::kTPCDExperimentClientState),
            static_cast<int>(utils::ExperimentState::kIneligible));
}

TEST_F(ExperimentManagerImplTest,
       ExperimentManager_SetIneligibleAfterDecisionCallDoesNothing) {
  Checkpoint checkpoint;
  {
    InSequence seq;
    EXPECT_CALL(mock_callback_, Run).Times(0);
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(mock_callback_, Run(true)).Times(1);
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(mock_callback_, Run(true)).Times(1);
  }

  TestingExperimentManagerImpl test_manager;
  test_manager.SetClientEligibility(/*is_eligible=*/true, mock_callback_.Get());

  checkpoint.Call(1);

  task_environment_.FastForwardBy(delay_time_);

  checkpoint.Call(2);

  test_manager.SetClientEligibility(/*is_eligible=*/false,
                                    mock_callback_.Get());

  EXPECT_EQ(prefs().GetInteger(prefs::kTPCDExperimentClientState),
            static_cast<int>(utils::ExperimentState::kEligible));
}

TEST_F(ExperimentManagerImplTest,
       ExperimentManager_SetEligibleAfterDecisionCallDoesNothing) {
  Checkpoint checkpoint;
  {
    InSequence seq;
    EXPECT_CALL(mock_callback_, Run).Times(0);
    EXPECT_CALL(checkpoint, Call(1));
    EXPECT_CALL(mock_callback_, Run(false)).Times(1);
    EXPECT_CALL(checkpoint, Call(2));
    EXPECT_CALL(mock_callback_, Run(false)).Times(1);
  }

  TestingExperimentManagerImpl test_manager;
  test_manager.SetClientEligibility(/*is_eligible=*/false,
                                    mock_callback_.Get());

  checkpoint.Call(1);

  task_environment_.FastForwardBy(delay_time_);

  checkpoint.Call(2);

  test_manager.SetClientEligibility(/*is_eligible=*/true, mock_callback_.Get());

  EXPECT_EQ(prefs().GetInteger(prefs::kTPCDExperimentClientState),
            static_cast<int>(utils::ExperimentState::kIneligible));
}

TEST_F(ExperimentManagerImplTest,
       ExperimentManager_PrefUnsetBeforeFinalDecisionIsMade) {
  TestingExperimentManagerImpl test_manager;
  test_manager.SetClientEligibility(/*is_eligible=*/false,
                                    mock_callback_.Get());
  // No callbacks run before the delay_time_ time completes.
  EXPECT_CALL(mock_callback_, Run).Times(0);
  // fastforward less than the full delay_time_.
  task_environment_.FastForwardBy(delay_time_ - base::Milliseconds(1));

  // pref value should still be "kUnknownEligibility" before delay_time
  // completes
  EXPECT_EQ(prefs().GetInteger(prefs::kTPCDExperimentClientState),
            static_cast<int>(utils::ExperimentState::kUnknownEligibility));
}

TEST_F(ExperimentManagerImplTest, PrefIneligibleReturnsEarly) {
  prefs().SetInteger(prefs::kTPCDExperimentClientState,
                     static_cast<int>(utils::ExperimentState::kIneligible));
  EXPECT_CALL(mock_callback_, Run(false)).Times(1);
  TestingExperimentManagerImpl().SetClientEligibility(/*is_eligible=*/true,
                                                      mock_callback_.Get());

  EXPECT_EQ(prefs().GetInteger(prefs::kTPCDExperimentClientState),
            static_cast<int>(utils::ExperimentState::kIneligible));
}

TEST_F(ExperimentManagerImplTest, IsClientEligible_PrefIsEligibleReturnsTrue) {
  prefs().SetInteger(prefs::kTPCDExperimentClientState,
                     static_cast<int>(utils::ExperimentState::kEligible));

  EXPECT_THAT(TestingExperimentManagerImpl().IsClientEligible(),
              Optional(true));
}

TEST_F(ExperimentManagerImplTest,
       IsClientEligible_PrefIsIneligibleReturnsFalse) {
  prefs().SetInteger(prefs::kTPCDExperimentClientState,
                     static_cast<int>(utils::ExperimentState::kIneligible));

  EXPECT_THAT(TestingExperimentManagerImpl().IsClientEligible(),
              Optional(false));
}

TEST_F(ExperimentManagerImplTest, IsClientEligible_PrefIsUnknownReturnsEmpty) {
  prefs().SetInteger(
      prefs::kTPCDExperimentClientState,
      static_cast<int>(utils::ExperimentState::kUnknownEligibility));

  EXPECT_EQ(TestingExperimentManagerImpl().IsClientEligible(), std::nullopt);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(ExperimentManagerImplTest, AshInternalProfile_NotCreated) {
  auto* internal_profile =
      profile_manager_.CreateTestingProfile(ash::kSigninBrowserContextBaseName);
  EXPECT_FALSE(ExperimentManagerImpl::GetForProfile(internal_profile));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// The parameter indicates whether to disable 3pcs.
class ExperimentManagerImplSyntheticTrialTest
    : public ExperimentManagerImplTestBase,
      public testing::WithParamInterface<bool> {};

TEST_P(ExperimentManagerImplSyntheticTrialTest, ProfileOnboardedSetsPref) {
  const bool disable_3p_cookies = GetParam();

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kCookieDeprecationFacilitatedTesting,
      {{kDisable3PCookiesName, disable_3p_cookies ? "true" : "false"},
       {kNeedOnboardingForSyntheticTrialName, "true"},
       {kEnableSilentOnboardingName, "true"}});

  TestingExperimentManagerImpl test_manager;
  test_manager.SetClientEligibility(/*is_eligible=*/true, mock_callback_.Get());
  EXPECT_CALL(mock_callback_, Run(true)).Times(1);
  task_environment_.FastForwardBy(delay_time_);

  EXPECT_EQ(prefs().GetInteger(prefs::kTPCDExperimentClientState),
            static_cast<int>(utils::ExperimentState::kEligible));

  test_manager.NotifyProfileTrackingProtectionOnboarded();
  EXPECT_EQ(prefs().GetInteger(prefs::kTPCDExperimentClientState),
            static_cast<int>(utils::ExperimentState::kOnboarded));
}

TEST_P(ExperimentManagerImplSyntheticTrialTest, CanRegister) {
  const bool disable_3p_cookies = GetParam();

  const struct {
    utils::ExperimentState experiment_state;
    bool expected;
    bool need_onboarding = false;
    bool enable_silent_onboarding = false;
  } kTestCases[] = {
      {
          .experiment_state = utils::ExperimentState::kUnknownEligibility,
          .expected = false,
      },
      {
          .experiment_state = utils::ExperimentState::kIneligible,
          .expected = true,
      },
      {
          .experiment_state = utils::ExperimentState::kEligible,
          .expected = true,
          .need_onboarding = false,
      },
      {
          .experiment_state = utils::ExperimentState::kEligible,
          .expected = !disable_3p_cookies,
          .need_onboarding = true,
      },
      {
          .experiment_state = utils::ExperimentState::kEligible,
          .expected = false,
          .need_onboarding = true,
          .enable_silent_onboarding = true,
      },
      {
          .experiment_state = utils::ExperimentState::kOnboarded,
          .expected = true,
      },
  };

  for (const auto& test_case : kTestCases) {
    base::test::ScopedFeatureList scope_feature_list;
    scope_feature_list.InitAndEnableFeatureWithParameters(
        features::kCookieDeprecationFacilitatedTesting,
        {{kDisable3PCookiesName, disable_3p_cookies ? "true" : "false"},
         {kNeedOnboardingForSyntheticTrialName,
          test_case.need_onboarding ? "true" : "false"},
         {kEnableSilentOnboardingName,
          test_case.enable_silent_onboarding ? "true" : "false"}});

    prefs().SetInteger(prefs::kTPCDExperimentClientState,
                       static_cast<int>(test_case.experiment_state));
    EXPECT_EQ(
        TestingExperimentManagerImpl().CanRegisterSyntheticTrialForTesting(),
        test_case.expected);
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         ExperimentManagerImplSyntheticTrialTest,
                         testing::Bool());

}  // namespace tpcd::experiment
