// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tpcd/experiment/experiment_manager_impl.h"

#include <optional>
#include <string>
#include <vector>

#include "base/location.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tpcd/experiment/tpcd_experiment_features.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/privacy_sandbox/tpcd_pref_names.h"
#include "components/privacy_sandbox/tpcd_utils.h"
#include "components/variations/active_field_trials.h"
#include "components/variations/hashing.h"
#include "components/variations/synthetic_trial_registry.h"
#include "components/variations/synthetic_trials.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tpcd::experiment {

using ::variations::HashName;

struct SyntheticTrialTestCase {
  utils::ExperimentState prev_state;
  bool new_state_eligible;
  std::optional<std::string> expected_group_name;
  std::string group_name_override;
  bool disable_3pcs = false;
  bool need_onboarding = false;
  bool enable_silent_onboarding = false;
};

constexpr char kEligibleGroupName[] = "eligible";
constexpr char kOverrideGroupName[] = "override";

class ExperimentManagerImplBrowserTest : public InProcessBrowserTest {
 public:
  ExperimentManagerImplBrowserTest(std::string group_name_override,
                                   bool disable_3pcs,
                                   bool need_onboarding,
                                   bool enable_silent_onboarding = false) {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kCookieDeprecationFacilitatedTesting,
        {{"label", kEligibleGroupName},
         {"synthetic_trial_group_override", group_name_override},
         {kDisable3PCookiesName, disable_3pcs ? "true" : "false"},
         {kNeedOnboardingForSyntheticTrialName,
          need_onboarding ? "true" : "false"},
         {kEnableSilentOnboardingName,
          enable_silent_onboarding ? "true" : "false"}});
  }

  void Wait() {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), kDecisionDelayTime.Get());
    run_loop.Run();
  }

  uint32_t GetSyntheticTrialGroupNameHash() {
    std::vector<variations::ActiveGroupId> synthetic_trials;
    g_browser_process->metrics_service()
        ->GetSyntheticTrialRegistry()
        ->GetSyntheticFieldTrialsOlderThan(base::TimeTicks::Now(),
                                           &synthetic_trials);

    uint32_t group_name_hash = 0u;
    for (const auto& trial : synthetic_trials) {
      if (trial.name == HashName(kSyntheticTrialName)) {
        group_name_hash = trial.group;
      }
    }
    return group_name_hash;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Android does not support PRE_ tests.
#if !BUILDFLAG(IS_ANDROID)
class ExperimentManagerImplSyntheticTrialTest
    : public ExperimentManagerImplBrowserTest,
      public testing::WithParamInterface<SyntheticTrialTestCase> {
 public:
  ExperimentManagerImplSyntheticTrialTest()
      : ExperimentManagerImplBrowserTest(GetParam().group_name_override,
                                         GetParam().disable_3pcs,
                                         GetParam().need_onboarding,
                                         GetParam().enable_silent_onboarding) {}
};

IN_PROC_BROWSER_TEST_P(ExperimentManagerImplSyntheticTrialTest,
                       PRE_RegistersSyntheticTrial) {
  Wait();

  // Set up the previous state in the local state prefs.
  g_browser_process->local_state()->SetInteger(
      prefs::kTPCDExperimentClientState,
      static_cast<int>(GetParam().prev_state));

  // Set up preconditions for the profile to be eligible.
  if (GetParam().new_state_eligible) {
    browser()->profile()->GetPrefs()->SetBoolean(
        ::prefs::kPrivacySandboxM1RowNoticeAcknowledged,
        GetParam().new_state_eligible);
    g_browser_process->local_state()->SetInt64(
        metrics::prefs::kInstallDate,
        (base::Time::Now() - base::Days(31)).ToTimeT());
  }
}

IN_PROC_BROWSER_TEST_P(ExperimentManagerImplSyntheticTrialTest,
                       RegistersSyntheticTrial) {
  // Delay to make sure `CaptureEligibilityInLocalStatePref` has run.
  Wait();

  // Verify that the user has been registered with the correct synthetic
  // trial group.

  uint32_t group_name_hash = GetSyntheticTrialGroupNameHash();
  if (const auto& expected_group_name = GetParam().expected_group_name;
      expected_group_name.has_value()) {
    ASSERT_NE(group_name_hash, 0u);
    EXPECT_EQ(group_name_hash, HashName(*expected_group_name));
  } else {
    ASSERT_EQ(group_name_hash, 0u);
  }
}

// Test every combination of (initial_state, new_state). If the prev_state is
// set, use that eligibility and ignore the new one. If the prev_state is
// unknown, use the new eligibility value.
const SyntheticTrialTestCase kTestCases[] = {
    {
        .prev_state = utils::ExperimentState::kUnknownEligibility,
        .new_state_eligible = false,
        .expected_group_name = kSyntheticTrialInvalidGroupName,
        .group_name_override = "",
    },
    {
        .prev_state = utils::ExperimentState::kUnknownEligibility,
        .new_state_eligible = false,
        .expected_group_name = kSyntheticTrialInvalidGroupName,
        .group_name_override = kOverrideGroupName,
    },
    {
        .prev_state = utils::ExperimentState::kUnknownEligibility,
        .new_state_eligible = true,
        .expected_group_name = kEligibleGroupName,
        .group_name_override = "",
    },
    {
        .prev_state = utils::ExperimentState::kUnknownEligibility,
        .new_state_eligible = true,
        .expected_group_name = kOverrideGroupName,
        .group_name_override = kOverrideGroupName,
    },
    {
        .prev_state = utils::ExperimentState::kIneligible,
        .new_state_eligible = false,
        .expected_group_name = kSyntheticTrialInvalidGroupName,
    },
    {
        .prev_state = utils::ExperimentState::kIneligible,
        .new_state_eligible = true,
        .expected_group_name = kSyntheticTrialInvalidGroupName,
    },
    {
        .prev_state = utils::ExperimentState::kEligible,
        .new_state_eligible = false,
        .expected_group_name = kEligibleGroupName,
    },
    {
        .prev_state = utils::ExperimentState::kEligible,
        .new_state_eligible = true,
        .expected_group_name = kEligibleGroupName,
    },
    {
        .prev_state = utils::ExperimentState::kEligible,
        .new_state_eligible = true,
        .expected_group_name = kEligibleGroupName,
        .need_onboarding = true,
        .enable_silent_onboarding = false,
    },
    {
        .prev_state = utils::ExperimentState::kEligible,
        .new_state_eligible = true,
        .expected_group_name = std::nullopt,
        .need_onboarding = true,
        .enable_silent_onboarding = true,
    },
    {
        .prev_state = utils::ExperimentState::kOnboarded,
        .new_state_eligible = true,
        .expected_group_name = kEligibleGroupName,
        .need_onboarding = true,
        .enable_silent_onboarding = true,
    },
    {
        .prev_state = utils::ExperimentState::kOnboarded,
        .new_state_eligible = true,
        .expected_group_name = kEligibleGroupName,
        .need_onboarding = false,
    },
    {
        .prev_state = utils::ExperimentState::kIneligible,
        .new_state_eligible = false,
        .expected_group_name = kSyntheticTrialInvalidGroupName,
        .disable_3pcs = true,
    },
    {
        .prev_state = utils::ExperimentState::kEligible,
        .new_state_eligible = true,
        .expected_group_name = std::nullopt,
        .disable_3pcs = true,
        .need_onboarding = true,
    },
    {
        .prev_state = utils::ExperimentState::kEligible,
        .new_state_eligible = true,
        .expected_group_name = kEligibleGroupName,
        .disable_3pcs = true,
        .need_onboarding = false,
    },
    {
        .prev_state = utils::ExperimentState::kOnboarded,
        .new_state_eligible = true,
        .expected_group_name = kEligibleGroupName,
        .disable_3pcs = true,
        .need_onboarding = true,
    },
    {
        .prev_state = utils::ExperimentState::kOnboarded,
        .new_state_eligible = true,
        .expected_group_name = kEligibleGroupName,
        .disable_3pcs = true,
        .need_onboarding = false,
    },
};

INSTANTIATE_TEST_SUITE_P(All,
                         ExperimentManagerImplSyntheticTrialTest,
                         testing::ValuesIn(kTestCases));

class ExperimentManagerImplDisable3PCsSyntheticTrialTest
    : public ExperimentManagerImplBrowserTest {
 public:
  ExperimentManagerImplDisable3PCsSyntheticTrialTest()
      : ExperimentManagerImplBrowserTest(
            /*group_name_override=*/"",
            /*disable_3pcs=*/true,
            /*need_onboarding=*/true) {}
};

IN_PROC_BROWSER_TEST_F(ExperimentManagerImplDisable3PCsSyntheticTrialTest,
                       PRE_ExistingProfilesRegistersSyntheticTrial) {
  Wait();

  // Set up the previous state in the local state prefs.
  g_browser_process->local_state()->SetInteger(
      prefs::kTPCDExperimentClientState,
      static_cast<int>(utils::ExperimentState::kOnboarded));
}

IN_PROC_BROWSER_TEST_F(ExperimentManagerImplDisable3PCsSyntheticTrialTest,
                       ExistingProfilesRegistersSyntheticTrial) {
  // Verify that the user has not been registered.
  uint32_t group_name_hash = GetSyntheticTrialGroupNameHash();
  ASSERT_NE(group_name_hash, 0u);
  EXPECT_EQ(group_name_hash, HashName(kEligibleGroupName));
}

class ExperimentManagerImplSilentOnboardingSyntheticTrialTest
    : public ExperimentManagerImplBrowserTest {
 public:
  ExperimentManagerImplSilentOnboardingSyntheticTrialTest()
      : ExperimentManagerImplBrowserTest(
            /*group_name_override=*/"",
            /*disable_3pcs=*/false,
            /*need_onboarding=*/true,
            /*enable_silent_onboarding=*/true) {}
};

IN_PROC_BROWSER_TEST_F(ExperimentManagerImplSilentOnboardingSyntheticTrialTest,
                       PRE_ExistingProfilesRegistersSyntheticTrial) {
  Wait();

  // Set up the previous state in the local state prefs.
  g_browser_process->local_state()->SetInteger(
      prefs::kTPCDExperimentClientState,
      static_cast<int>(utils::ExperimentState::kOnboarded));
}

IN_PROC_BROWSER_TEST_F(ExperimentManagerImplSilentOnboardingSyntheticTrialTest,
                       ExistingProfilesRegistersSyntheticTrial) {
  // Verify that the user has not been registered.
  uint32_t group_name_hash = GetSyntheticTrialGroupNameHash();
  ASSERT_NE(group_name_hash, 0u);
  EXPECT_EQ(group_name_hash, HashName(kEligibleGroupName));
}

#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace tpcd::experiment
