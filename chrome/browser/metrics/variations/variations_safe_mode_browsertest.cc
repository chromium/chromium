// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The tests in this file verify the behavior of variations safe mode. The tests
// should be kept in sync with those in ios/chrome/browser/variations/
// variations_safe_mode_egtest.mm.

#include <string>

#include "base/metrics/field_trial.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/browser_process.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/metrics/clean_exit_beacon.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/variations/metrics.h"
#include "components/variations/pref_names.h"
#include "components/variations/variations_test_utils.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations {
namespace {

// Sets |local_state|'s seed and seed signature prefs to a valid seed-signature
// pair. If |use_safe_seed_prefs| is true, then uses the safe seed prefs.
void StoreTestSeedAndSignature(PrefService* local_state,
                               bool use_safe_seed_prefs) {
  const std::string seed_pref = use_safe_seed_prefs
                                    ? prefs::kVariationsSafeCompressedSeed
                                    : prefs::kVariationsCompressedSeed;
  local_state->SetString(seed_pref, kCompressedBase64TestSeedData);

  const std::string signature_pref = use_safe_seed_prefs
                                         ? prefs::kVariationsSafeSeedSignature
                                         : prefs::kVariationsSeedSignature;
  local_state->SetString(signature_pref, kBase64TestSeedSignature);
}

// Simulates a crash by forcing Chrome to fail to exit cleanly.
void SimulateCrash(PrefService* local_state) {
  local_state->SetBoolean(metrics::prefs::kStabilityExitedCleanly, false);
  metrics::CleanExitBeacon::SkipCleanShutdownStepsForTesting();
}

}  // namespace

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
  StoreTestSeedAndSignature(local_state, /*use_safe_seed_prefs=*/true);
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
  histogram_tester_.ExpectUniqueSample(
      "Variations.SafeMode.FellBackToSafeMode2", true, 1);

  // Verify that there is a field trial associated with the sole test seed
  // study, |kTestSeedStudyName|.
  EXPECT_TRUE(base::FieldTrialList::TrialExists(kTestSeedStudyName));
}

IN_PROC_BROWSER_TEST_F(VariationsSafeModeBrowserTest,
                       PRE_FetchFailuresTriggerSafeMode) {
  // The PRE test mechanism is used to set prefs in the local state file before
  // the next browser test runs. No InProcessBrowserTest functions allow this
  // pref to be set early enough to be read by the variations code, which runs
  // very early during startup.
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetInteger(prefs::kVariationsFailedToFetchSeedStreak, 25);
  StoreTestSeedAndSignature(local_state, /*use_safe_seed_prefs=*/true);
}

IN_PROC_BROWSER_TEST_F(VariationsSafeModeBrowserTest,
                       FetchFailuresTriggerSafeMode) {
  histogram_tester_.ExpectUniqueSample(
      "Variations.SafeMode.Streak.FetchFailures", 25, 1);

  // Verify that Chrome fell back to a safe seed, which happens during browser
  // test setup.
  histogram_tester_.ExpectUniqueSample(
      "Variations.SafeMode.LoadSafeSeed.Result", LoadSeedResult::kSuccess, 1);
  histogram_tester_.ExpectUniqueSample(
      "Variations.SafeMode.FellBackToSafeMode2", true, 1);

  // Verify that there is a field trial associated with the sole test seed
  // study, |kTestSeedStudyName|.
  EXPECT_TRUE(base::FieldTrialList::TrialExists(kTestSeedStudyName));
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
  StoreTestSeedAndSignature(local_state, /*use_safe_seed_prefs=*/false);
}

IN_PROC_BROWSER_TEST_F(VariationsSafeModeBrowserTest, DoNotTriggerSafeMode) {
  histogram_tester_.ExpectUniqueSample("Variations.SafeMode.Streak.Crashes", 2,
                                       1);
  histogram_tester_.ExpectUniqueSample(
      "Variations.SafeMode.Streak.FetchFailures", 24, 1);

  // Verify that Chrome applied the latest seed.
  histogram_tester_.ExpectUniqueSample("Variations.SeedLoadResult",
                                       LoadSeedResult::kSuccess, 1);

  // Verify that Chrome did not fall back to a safe seed.
  histogram_tester_.ExpectUniqueSample(
      "Variations.SafeMode.FellBackToSafeMode2", false, 1);
}

}  // namespace variations
