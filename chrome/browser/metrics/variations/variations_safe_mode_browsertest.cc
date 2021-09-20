// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The tests in this file verify the behavior of variations safe mode. The tests
// should be kept in sync with those in ios/chrome/browser/variations/
// variations_safe_mode_egtest.mm.

#include <string>

#include "base/base_switches.h"
#include "base/containers/contains.h"
#include "base/metrics/field_trial.h"
#include "base/path_service.h"
#include "base/ranges/ranges.h"
#include "base/strings/strcat.h"
#include "base/test/launcher/test_launcher.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_switches.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/metrics/clean_exit_beacon.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "components/variations/metrics.h"
#include "components/variations/pref_names.h"
#include "components/variations/service/variations_field_trial_creator.h"
#include "components/variations/service/variations_safe_mode_constants.h"
#include "components/variations/service/variations_service.h"
#include "components/variations/variations_switches.h"
#include "components/variations/variations_test_utils.h"
#include "content/public/common/content_switches.h"
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
  histogram_tester_.ExpectUniqueSample("Variations.SeedUsage",
                                       SeedUsage::kSafeSeedUsed, 1);

  // Verify that there is a field trial associated with the sole test seed
  // study, |variations::kTestSeedStudyName|.
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
  histogram_tester_.ExpectUniqueSample("Variations.SeedUsage",
                                       SeedUsage::kSafeSeedUsed, 1);

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

  // Verify that Chrome applied the regular seed.
  histogram_tester_.ExpectUniqueSample("Variations.SeedLoadResult",
                                       LoadSeedResult::kSuccess, 1);
  histogram_tester_.ExpectUniqueSample("Variations.SeedUsage",
                                       SeedUsage::kRegularSeedUsed, 1);
}

// This test code is programmatically launched by the SafeModeEndToEnd
// test below. Its primary purpose is to provide an entry-point by
// which the SafeModeEndToEnd test can cause the Field Trial Setup
// code to be exercised. For some launches, the setup code is expected
// to crash before reaching the test body; the test body simply verifies
// that the test is using the user-data-dir configured on the command-line.
//
// The MANUAL_ prefix prevents the test from running unless explicitly
// invoked.
IN_PROC_BROWSER_TEST_F(VariationsSafeModeBrowserTest, MANUAL_SubTest) {
  // Validate that Chrome is running with the user-data-dir specified on the
  // command-line.
  base::FilePath expected_user_data_dir =
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          ::switches::kUserDataDir);
  base::FilePath actual_user_data_dir;
  ASSERT_TRUE(
      base::PathService::Get(chrome::DIR_USER_DATA, &actual_user_data_dir));
  ASSERT_FALSE(expected_user_data_dir.empty());
  ASSERT_FALSE(actual_user_data_dir.empty());
  ASSERT_EQ(actual_user_data_dir, expected_user_data_dir);
}

namespace {

class FieldTrialTest : public ::testing::TestWithParam<std::string> {
 public:
  void SetUp() override {
    ::testing::TestWithParam<std::string>::SetUp();
    metrics::CleanExitBeacon::SkipCleanShutdownStepsForTesting();

    pref_registry_ = base::MakeRefCounted<PrefRegistrySimple>();
    metrics::MetricsService::RegisterPrefs(pref_registry_.get());
    variations::VariationsService::RegisterPrefs(pref_registry_.get());

    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    user_data_dir_ = temp_dir_.GetPath().AppendASCII("user-data-dir");
    pref_service_factory_.set_user_prefs(base::MakeRefCounted<JsonPrefStore>(
        user_data_dir_.AppendASCII("Local State")));
  }

 protected:
  const std::string& field_trial_group() const { return GetParam(); }
  const base::FilePath& user_data_dir() const { return user_data_dir_; }

  bool IsSuccessfulSubTestOutput(const std::string& output) {
    static const char* const kSubTestSuccessStrings[] = {
        "Running 1 test from 1 test suite",
        "OK ] VariationsSafeModeBrowserTest.MANUAL_SubTest",
        "1 test from VariationsSafeModeBrowserTest",
        "1 test from 1 test suite ran",
    };
    return base::ranges::all_of(kSubTestSuccessStrings, [&](const char* s) {
      return base::Contains(output, s);
    });
  }

  bool IsCrashingSubTestOutput(const std::string& output) {
    const char* const kSubTestCrashStrings[] = {
        "Running 1 test from 1 test suite",
        "VariationsSafeModeBrowserTest.MANUAL_SubTest",
        "Check failed: crash_for_testing",
    };
    return base::ranges::all_of(kSubTestCrashStrings, [&](const char* s) {
      return base::Contains(output, s);
    });
  }

  void RunAndExpectSuccessfulSubTest(
      const base::CommandLine& sub_test_command) {
    std::string output;
    base::GetAppOutputAndError(sub_test_command, &output);
    EXPECT_TRUE(IsSuccessfulSubTestOutput(output))
        << "Did not find success signals in output:\n"
        << output;
  }

  void RunAndExpectCrashingSubTest(const base::CommandLine& sub_test_command) {
    std::string output;
    base::GetAppOutputAndError(sub_test_command, &output);
    EXPECT_FALSE(IsSuccessfulSubTestOutput(output))
        << "Expected crash but found success signals in output:\n"
        << output;
    EXPECT_TRUE(IsCrashingSubTestOutput(output))
        << "Did not find crash signals in output:\n"
        << output;
  }

  std::unique_ptr<PrefService> LoadLocalState() {
    return pref_service_factory_.Create(pref_registry_);
  }

  std::unique_ptr<metrics::CleanExitBeacon> LoadCleanExitBeacon(
      PrefService* pref_service) {
    static constexpr wchar_t kDummyWindowsRegistryKey[] = L"";
    auto clean_exit_beacon = std::make_unique<metrics::CleanExitBeacon>(
        kDummyWindowsRegistryKey, user_data_dir(), pref_service);
    clean_exit_beacon->Initialize();
    return clean_exit_beacon;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<PrefRegistrySimple> pref_registry_;
  PrefServiceFactory pref_service_factory_;
  base::ScopedTempDir temp_dir_;
  base::FilePath user_data_dir_;
};

}  // namespace

TEST_P(FieldTrialTest, ExtendedSafeModeEndToEnd) {
  SCOPED_TRACE(field_trial_group());

  // Reuse the browser_tests binary (i.e., that this test code is in), to
  // manually run the sub-test.
  base::CommandLine sub_test =
      base::CommandLine(base::CommandLine::ForCurrentProcess()->GetProgram());

  // Run the sub-test in the |user_data_dir()| allocated for the test case.
  sub_test.AppendSwitchASCII(base::kGTestFilterFlag,
                             "VariationsSafeModeBrowserTest.MANUAL_SubTest");
  sub_test.AppendSwitch(::switches::kRunManualTestsFlag);
  sub_test.AppendSwitch(::switches::kSingleProcessTests);
  sub_test.AppendSwitchPath(::switches::kUserDataDir, user_data_dir());

  // Select the extended variations safe mode field trial group. The "*"
  // prefix forces the experiment/trial state to "active" at startup.
  sub_test.AppendSwitchASCII(::switches::kForceFieldTrials,
                             base::StrCat({"*", kExtendedSafeModeTrial, "/",
                                           field_trial_group(), "/"}));

  // Explicitly avoid any terminal control characters in the output.
  sub_test.AppendSwitchASCII("gtest_color", "no");

  // Initial sub-test run should be successful.
  RunAndExpectSuccessfulSubTest(sub_test);

  // Add command-line switch to force crash during metric initialization.
  // TODO(crbug/1249256): inject variations seed into user-data-dir that
  // enables this feature instead of using altered command line.
  base::CommandLine crashing_sub_test = sub_test;
  crashing_sub_test.AppendSwitchASCII(
      ::switches::kEnableFeatures, kForceFieldTrialSetupCrashForTesting.name);

  SetUpExtendedSafeModeExperiment(field_trial_group());

  // The next three runs of the sub-test should crash...
  for (int expected_crash_streak = 1;
       expected_crash_streak <= kCrashStreakThreshold;
       ++expected_crash_streak) {
    RunAndExpectCrashingSubTest(crashing_sub_test);
    auto local_state = LoadLocalState();
    auto clean_exit_beacon = LoadCleanExitBeacon(local_state.get());
    ASSERT_TRUE(clean_exit_beacon != nullptr);
    ASSERT_FALSE(clean_exit_beacon->exited_cleanly());
    EXPECT_EQ(expected_crash_streak,
              local_state->GetInteger(prefs::kVariationsCrashStreak));
  }

  // Until safe mode kicks in.
  RunAndExpectSuccessfulSubTest(sub_test);
}

INSTANTIATE_TEST_CASE_P(
    VariationsSafeModeBrowserTest,
    FieldTrialTest,
    ::testing::Values(kSignalAndWriteSynchronouslyViaPrefServiceGroup,
                      kSignalAndWriteViaFileUtilGroup));
}  // namespace variations
