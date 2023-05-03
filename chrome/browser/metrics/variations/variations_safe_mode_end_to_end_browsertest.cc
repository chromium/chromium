// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The tests in this file verify the behavior of variations safe mode. The tests
// should be kept in sync with those in ios/chrome/browser/variations/
// variations_safe_mode_egtest.mm.

#include <string>

#include "base/atomic_sequence_num.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/metrics/field_trial.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/ranges/ranges.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/launcher/test_launcher.h"
#include "base/test/task_environment.h"
#include "base/test/test_switches.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/metrics/clean_exit_beacon.h"
#include "components/metrics/metrics_service.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "components/variations/pref_names.h"
#include "components/variations/service/variations_field_trial_creator.h"
#include "components/variations/service/variations_service.h"
#include "components/variations/variations_switches.h"
#include "components/variations/variations_test_utils.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_POSIX)
#include <sys/wait.h>
#endif

namespace variations {

class VariationsSafeModeEndToEndBrowserTestHelper
    : public InProcessBrowserTest {
 public:
  VariationsSafeModeEndToEndBrowserTestHelper() { DisableTestingConfig(); }
  ~VariationsSafeModeEndToEndBrowserTestHelper() override = default;
};

// This test code is programmatically launched by the SafeModeEndToEnd
// test below. Its primary purpose is to provide an entry-point by
// which the SafeModeEndToEnd test can cause the Field Trial Setup
// code to be exercised. For some launches, the setup code is expected
// to crash before reaching the test body; the test body simply verifies
// that the test is using the user-data-dir configured on the command-line.
//
// The MANUAL_ prefix prevents the test from running unless explicitly
// invoked.
IN_PROC_BROWSER_TEST_F(VariationsSafeModeEndToEndBrowserTestHelper,
                       MANUAL_SubTest) {
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
  ASSERT_TRUE(base::EndsWith(actual_user_data_dir.value(),
                             expected_user_data_dir.value()));

  // If the test makes it this far, then either it's the first run of the
  // test, or the safe seed was used, or it is the run using the null seed.
  const int crash_streak = g_browser_process->local_state()->GetInteger(
      prefs::kVariationsCrashStreak);
  const bool is_first_run = (crash_streak == 0);
  const bool is_null_seed = (crash_streak == kCrashStreakNullSeedThreshold);
  const bool safe_seed_was_used =
      FieldTrialListHasAllStudiesFrom(kTestSeedData);
  EXPECT_NE(is_first_run || is_null_seed, safe_seed_was_used)  // ==> XOR
      << "crash_streak=" << crash_streak;
}

class VariationsSafeModeEndToEndBrowserTest : public ::testing::Test {
 public:
  void SetUp() override {
    ::testing::Test::SetUp();
    metrics::CleanExitBeacon::SkipCleanShutdownStepsForTesting();

    pref_registry_ = base::MakeRefCounted<PrefRegistrySimple>();
    metrics::MetricsService::RegisterPrefs(pref_registry_.get());
    variations::VariationsService::RegisterPrefs(pref_registry_.get());

    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    user_data_dir_ = temp_dir_.GetPath().AppendASCII("user-data-dir");
    local_state_file_ = user_data_dir_.AppendASCII("Local State");
  }

 protected:
  base::CommandLine SetUpSubTest() {
    // Reuse the browser_tests binary (i.e., that this test code is in), to
    // manually run the sub-test.
    base::CommandLine sub_test =
        base::CommandLine(base::CommandLine::ForCurrentProcess()->GetProgram());

    // Run the manual sub-test in the |user_data_dir()| allocated for this test.
    sub_test.AppendSwitchASCII(
        base::kGTestFilterFlag,
        "VariationsSafeModeEndToEndBrowserTestHelper.MANUAL_SubTest");
    sub_test.AppendSwitch(::switches::kRunManualTestsFlag);
    sub_test.AppendSwitch(::switches::kSingleProcessTests);
    sub_test.AppendSwitchPath(::switches::kUserDataDir, user_data_dir());

    // Assign the test environment to be on the Canary channel. This ensures
    // compatibility with the crashing study in the seed.
    sub_test.AppendSwitchASCII(switches::kFakeVariationsChannel, "canary");

    // Explicitly avoid any terminal control characters in the output.
    sub_test.AppendSwitchASCII("gtest_color", "no");
    return sub_test;
  }

  const base::FilePath& user_data_dir() const { return user_data_dir_; }
  const base::FilePath& local_state_file() const { return local_state_file_; }

  const base::FilePath CopyOfLocalStateFile() const {
    static base::AtomicSequenceNumber suffix;
    base::FilePath copy_of_local_state_file = temp_dir_.GetPath().AppendASCII(
        base::StringPrintf("local-state-copy-%d.json", suffix.GetNext()));
    base::CopyFile(local_state_file(), copy_of_local_state_file);
    return copy_of_local_state_file;
  }

  void RunAndExpectSuccessfulSubTest(
      const base::CommandLine& sub_test_command) {
    std::string output;
    int exit_code;
    base::GetAppOutputWithExitCode(sub_test_command, &output, &exit_code);
    EXPECT_EQ(0, exit_code) << "Did not get successful exit code";
  }

  void RunAndExpectCrashingSubTest(const base::CommandLine& sub_test_command) {
    std::string output;
    int exit_code;
    base::GetAppOutputWithExitCode(sub_test_command, &output, &exit_code);
#if BUILDFLAG(IS_POSIX)
    ASSERT_TRUE(WIFSIGNALED(exit_code));
    // Posix only defines 7-bit exit codes.
    EXPECT_EQ(0x7E57C0D3 & 0x7F, WTERMSIG(exit_code))
        << "Did not get crash exit code";
#else
    EXPECT_EQ(0x7E57C0D3, exit_code) << "Did not get crash exit code";
#endif
  }

  std::unique_ptr<PrefService> LoadLocalState(const base::FilePath& path) {
    PrefServiceFactory pref_service_factory;
    pref_service_factory.set_async(false);
    pref_service_factory.set_user_prefs(
        base::MakeRefCounted<JsonPrefStore>(path));
    return pref_service_factory.Create(pref_registry_);
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
  base::ScopedTempDir temp_dir_;
  base::FilePath user_data_dir_;
  base::FilePath local_state_file_;
};

TEST_F(VariationsSafeModeEndToEndBrowserTest, ExtendedSafeSeedEndToEnd) {
  base::CommandLine sub_test = SetUpSubTest();

  // Initial sub-test run should be successful.
  RunAndExpectSuccessfulSubTest(sub_test);

  // To speed up the test, skip the first k-1 crashing runs.
  const int initial_crash_count = kCrashStreakSafeSeedThreshold - 1;

  // Inject the safe and crashing seeds into the Local State of |sub_test|.
  {
    auto local_state = LoadLocalState(local_state_file());
    local_state->SetInteger(prefs::kVariationsCrashStreak, initial_crash_count);
    WriteSeedData(local_state.get(), kTestSeedData, kSafeSeedPrefKeys);
    WriteSeedData(local_state.get(), kCrashingSeedData, kRegularSeedPrefKeys);
  }

  // The next run will be |kCrashStreakSafeSeedThreshold| crashing runs of the
  // sub-test.
  {
    RunAndExpectCrashingSubTest(sub_test);
    auto local_state = LoadLocalState(CopyOfLocalStateFile());
    auto clean_exit_beacon = LoadCleanExitBeacon(local_state.get());
    ASSERT_TRUE(clean_exit_beacon != nullptr);
    ASSERT_FALSE(clean_exit_beacon->exited_cleanly());
    ASSERT_EQ(kCrashStreakSafeSeedThreshold,
              local_state->GetInteger(prefs::kVariationsCrashStreak));
  }

  // Do another run and verify that safe mode kicks in, preventing the crash.
  RunAndExpectSuccessfulSubTest(sub_test);
}

TEST_F(VariationsSafeModeEndToEndBrowserTest, ExtendedNullSeedEndToEnd) {
  base::CommandLine sub_test = SetUpSubTest();

  // Initial sub-test run should be successful.
  RunAndExpectSuccessfulSubTest(sub_test);

  // To speed up the test, skip the first k-1 crashing runs.
  const int initial_crash_count = kCrashStreakNullSeedThreshold - 1;

  // Inject the crashing seeds for both Regular and Safe.
  {
    auto local_state = LoadLocalState(local_state_file());
    local_state->SetInteger(prefs::kVariationsCrashStreak, initial_crash_count);
    WriteSeedData(local_state.get(), kCrashingSeedData, kSafeSeedPrefKeys);
    WriteSeedData(local_state.get(), kCrashingSeedData, kRegularSeedPrefKeys);
  }

  // The next run will be |kCrashStreakNullSeedThreshold| crashing runs of the
  // sub-test.
  {
    RunAndExpectCrashingSubTest(sub_test);
    auto local_state = LoadLocalState(CopyOfLocalStateFile());
    auto clean_exit_beacon = LoadCleanExitBeacon(local_state.get());
    ASSERT_TRUE(clean_exit_beacon != nullptr);
    ASSERT_FALSE(clean_exit_beacon->exited_cleanly());
    ASSERT_EQ(kCrashStreakNullSeedThreshold,
              local_state->GetInteger(prefs::kVariationsCrashStreak));
  }

  // Should use null seed, preventing the crash.
  RunAndExpectSuccessfulSubTest(sub_test);
}

}  // namespace variations
