// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The tests in this file verify the behavior of variations safe mode. The tests
// should be kept in sync with those in ios/chrome/browser/variations/
// variations_safe_mode_egtest.mm.

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/browser_process.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/variations/metrics.h"
#include "components/variations/pref_names.h"
#include "components/variations/service/variations_field_trial_creator.h"
#include "components/variations/variations_test_utils.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations {

class VariationsSafeModeBrowserTest : public InProcessBrowserTest {
 public:
  VariationsSafeModeBrowserTest() { DisableTestingConfig(); }
  ~VariationsSafeModeBrowserTest() override = default;

 protected:
  base::HistogramTester histogram_tester_;
};

IN_PROC_BROWSER_TEST_F(VariationsSafeModeBrowserTest,
                       PRE_PRE_PRE_ThreeCrashesTriggerSafeMode) {
  // The PRE test mechanism is used to set prefs in the local state file before
  // the next browser test runs. No InProcessBrowserTest functions allow this
  // pref to be set early enough to be read by the variations code, which runs
  // very early during startup.
  PrefService* local_state = g_browser_process->local_state();
  WriteSeedData(local_state, kTestSeedData, kSafeSeedPrefKeys);
  SimulateCrash(local_state);
}

IN_PROC_BROWSER_TEST_F(VariationsSafeModeBrowserTest,
                       PRE_PRE_ThreeCrashesTriggerSafeMode) {
  SimulateCrash(g_browser_process->local_state());
}

IN_PROC_BROWSER_TEST_F(VariationsSafeModeBrowserTest,
                       PRE_ThreeCrashesTriggerSafeMode) {
  SimulateCrash(g_browser_process->local_state());
}

IN_PROC_BROWSER_TEST_F(VariationsSafeModeBrowserTest,
                       ThreeCrashesTriggerSafeMode) {
  EXPECT_EQ(g_browser_process->local_state()->GetInteger(
                prefs::kVariationsCrashStreak),
            3);
  histogram_tester_.ExpectUniqueSample("Variations.SafeMode.Streak.Crashes", 3,
                                       1);

  // Verify that Chrome fell back to a safe seed, which happens during browser
  // test setup.
  histogram_tester_.ExpectUniqueSample(
      "Variations.SafeMode.LoadSafeSeed.Result", LoadSeedResult::kSuccess, 1);
  histogram_tester_.ExpectUniqueSample("Variations.SeedUsage",
                                       SeedUsage::kSafeSeedUsed, 1);

  // Verify that |kTestSeedData| has been applied.
  EXPECT_TRUE(FieldTrialListHasAllStudiesFrom(kTestSeedData));
}

IN_PROC_BROWSER_TEST_F(VariationsSafeModeBrowserTest,
                       PRE_FetchFailuresTriggerSafeMode) {
  // The PRE test mechanism is used to set prefs in the local state file before
  // the next browser test runs. No InProcessBrowserTest functions allow this
  // pref to be set early enough to be read by the variations code, which runs
  // very early during startup.
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetInteger(prefs::kVariationsFailedToFetchSeedStreak, 25);
  WriteSeedData(local_state, kTestSeedData, kSafeSeedPrefKeys);
}

IN_PROC_BROWSER_TEST_F(VariationsSafeModeBrowserTest,
                       FetchFailuresTriggerSafeMode) {
  histogram_tester_.ExpectUniqueSample(
      "Variations.SafeMode.Streak.FetchFailures", 25, 1);

  // Verify that Chrome fell back to a safe seed, which happens during browser
  // test setup.
  histogram_tester_.ExpectUniqueSample(
      "Variations.SafeMode.LoadSafeSeed.Result", LoadSeedResult::kSuccess, 1);
  histogram_tester_.ExpectUniqueSample("Variations.SeedUsage",
                                       SeedUsage::kSafeSeedUsed, 1);

  // Verify that |kTestSeedData| has been applied.
  EXPECT_TRUE(FieldTrialListHasAllStudiesFrom(kTestSeedData));
}

IN_PROC_BROWSER_TEST_F(VariationsSafeModeBrowserTest,
                       PRE_DoNotTriggerSafeMode) {
  // The PRE test mechanism is used to set prefs in the local state file before
  // the next browser test runs. No InProcessBrowserTest functions allow this
  // pref to be set early enough to be read by the variations code, which runs
  // very early during startup.
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetInteger(prefs::kVariationsCrashStreak, 2);
  local_state->SetInteger(prefs::kVariationsFailedToFetchSeedStreak, 24);
  WriteSeedData(local_state, kTestSeedData, kRegularSeedPrefKeys);
}

IN_PROC_BROWSER_TEST_F(VariationsSafeModeBrowserTest, DoNotTriggerSafeMode) {
  histogram_tester_.ExpectUniqueSample("Variations.SafeMode.Streak.Crashes", 2,
                                       1);
  histogram_tester_.ExpectUniqueSample(
      "Variations.SafeMode.Streak.FetchFailures", 24, 1);

  // Verify that Chrome applied the regular seed.
  histogram_tester_.ExpectUniqueSample("Variations.SeedLoadResult",
                                       LoadSeedResult::kSuccess, 1);
  histogram_tester_.ExpectUniqueSample("Variations.SeedUsage",
                                       SeedUsage::kRegularSeedUsed, 1);
}

}  // namespace variations
