// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tpcd/experiment/experiment_manager_impl.h"

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/strings/to_string.h"
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
         {kDisable3PCookiesName, base::ToString(disable_3pcs)},
         {kNeedOnboardingForSyntheticTrialName,
          base::ToString(need_onboarding)},
         {kEnableSilentOnboardingName,
          base::ToString(enable_silent_onboarding)}});
  }

  void Wait() {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), kDecisionDelayTime.Get());
    run_loop.Run();
  }

  uint32_t GetSyntheticTrialGroupNameHash() {
    std::vector<variations::ActiveGroupId> synthetic_trials =
        g_browser_process->metrics_service()
            ->GetSyntheticTrialRegistry()
            ->GetCurrentSyntheticFieldTrialsForTest();

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
#if !BUILDFLAG(IS_CHROMEOS)

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
#endif  // !BUILDFLAG(IS_CHROMEOS)

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
