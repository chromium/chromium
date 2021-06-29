// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/base64.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
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
#include "third_party/zlib/google/compression_utils.h"

namespace {

// Returns a base64-encoded compressed serialized form of a VariationsSeed.
std::string GetTestSeedForPrefs() {
  std::string serialized_seed;
  base::Base64Decode(variations::kUncompressedBase64TestSeedData,
                     &serialized_seed);

  std::string compressed_seed_data;
  compression::GzipCompress(serialized_seed, &compressed_seed_data);

  std::string base64_seed_data;
  base::Base64Encode(compressed_seed_data, &base64_seed_data);
  return base64_seed_data;
}

// Sets |local_state|'s seed and seed signature prefs to a valid seed-signature
// pair. If |use_safe_seed_prefs| is true, then uses the safe seed prefs.
void StoreTestSeedAndSignature(PrefService* local_state,
                               bool use_safe_seed_prefs) {
  const std::string seed_pref =
      use_safe_seed_prefs ? variations::prefs::kVariationsSafeCompressedSeed
                          : variations::prefs::kVariationsCompressedSeed;
  local_state->SetString(seed_pref, GetTestSeedForPrefs());

  const std::string signature_pref =
      use_safe_seed_prefs ? variations::prefs::kVariationsSafeSeedSignature
                          : variations::prefs::kVariationsSeedSignature;
  local_state->SetString(signature_pref, variations::kBase64TestSeedSignature);
}

// Simulates a crash by forcing Chrome to fail to exit cleanly.
void SimulateCrash(PrefService* local_state) {
  local_state->SetBoolean(metrics::prefs::kStabilityExitedCleanly, false);
  metrics::CleanExitBeacon::SkipCleanShutdownStepsForTesting();
}

}  // namespace

class VariationsSafeModeBrowserTest : public InProcessBrowserTest {
 public:
  VariationsSafeModeBrowserTest() { variations::DisableTestingConfig(); }
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
                variations::prefs::kVariationsCrashStreak),
            3);
  histogram_tester_.ExpectUniqueSample("Variations.SafeMode.Streak.Crashes", 3,
                                       1);

  // Verify that Chrome fell back to a safe seed, which happens during browser
  // test setup.
  histogram_tester_.ExpectUniqueSample(
      "Variations.SafeMode.LoadSafeSeed.Result",
      variations::LoadSeedResult::SUCCESS, 1);
  histogram_tester_.ExpectUniqueSample(
      "Variations.SafeMode.FellBackToSafeMode2", true, 1);
}

IN_PROC_BROWSER_TEST_F(VariationsSafeModeBrowserTest,
                       PRE_FetchFailuresTriggerSafeMode) {
  // The PRE test mechanism is used to set prefs in the local state file before
  // the next browser test runs. No InProcessBrowserTest functions allow this
  // pref to be set early enough to be read by the variations code, which runs
  // very early during startup.
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetInteger(variations::prefs::kVariationsFailedToFetchSeedStreak,
                          25);
  StoreTestSeedAndSignature(local_state, /*use_safe_seed_prefs=*/true);
}

IN_PROC_BROWSER_TEST_F(VariationsSafeModeBrowserTest,
                       FetchFailuresTriggerSafeMode) {
  histogram_tester_.ExpectUniqueSample(
      "Variations.SafeMode.Streak.FetchFailures", 25, 1);

  // Verify that Chrome fell back to a safe seed, which happens during browser
  // test setup.
  histogram_tester_.ExpectUniqueSample(
      "Variations.SafeMode.LoadSafeSeed.Result",
      variations::LoadSeedResult::SUCCESS, 1);
  histogram_tester_.ExpectUniqueSample(
      "Variations.SafeMode.FellBackToSafeMode2", true, 1);
}

IN_PROC_BROWSER_TEST_F(VariationsSafeModeBrowserTest,
                       PRE_DoNotTriggerSafeMode) {
  // The PRE test mechanism is used to set prefs in the local state file before
  // the next browser test runs. No InProcessBrowserTest functions allow this
  // pref to be set early enough to be read by the variations code, which runs
  // very early during startup.
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetInteger(variations::prefs::kVariationsCrashStreak, 2);
  local_state->SetInteger(variations::prefs::kVariationsFailedToFetchSeedStreak,
                          24);
  StoreTestSeedAndSignature(local_state, /*use_safe_seed_prefs=*/false);
}

IN_PROC_BROWSER_TEST_F(VariationsSafeModeBrowserTest, DoNotTriggerSafeMode) {
  histogram_tester_.ExpectUniqueSample("Variations.SafeMode.Streak.Crashes", 2,
                                       1);
  histogram_tester_.ExpectUniqueSample(
      "Variations.SafeMode.Streak.FetchFailures", 24, 1);

  // Verify that Chrome applied the latest seed.
  histogram_tester_.ExpectUniqueSample("Variations.SeedLoadResult",
                                       variations::LoadSeedResult::SUCCESS, 1);

  // Verify that Chrome did not fall back to a safe seed.
  histogram_tester_.ExpectUniqueSample(
      "Variations.SafeMode.FellBackToSafeMode2", false, 1);
}
