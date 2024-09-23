// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_file.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/test/launcher/test_launcher.h"
#include "base/test/task_environment.h"
#include "base/test/test_switches.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/metrics/metrics_service.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service_factory.h"
#include "components/variations/active_field_trials.h"
#include "components/variations/hashing.h"
#include "components/variations/limited_entropy_mode_gate.h"
#include "components/variations/pref_names.h"
#include "components/variations/service/limited_entropy_synthetic_trial.h"
#include "components/variations/service/variations_service.h"
#include "components/variations/synthetic_trial_registry.h"
#include "components/variations/variations_switches.h"
#include "components/variations/variations_test_utils.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations {

const char kTestStudyName[] = "TestStudy";

namespace {

VariationsSeed CreateTestSeedWithLimitedEntropyLayer() {
  VariationsSeed seed;
  seed.set_serial_number("999");

  auto* layer = seed.add_layers();
  layer->set_id(1);
  layer->set_num_slots(100);
  layer->set_entropy_mode(Layer::LIMITED);

  auto* layer_member = layer->add_members();
  layer_member->set_id(1);
  auto* slot = layer_member->add_slots();
  slot->set_start(0);
  slot->set_end(99);

  auto* study = seed.add_study();
  study->set_name(kTestStudyName);
  study->set_consistency(Study_Consistency_PERMANENT);

  auto* filter = study->mutable_filter();
  filter->set_min_version("91.*");
  filter->add_channel(Study_Channel_CANARY);
  filter->add_channel(Study_Channel_BETA);
  filter->add_channel(Study_Channel_DEV);
  filter->add_channel(Study_Channel_STABLE);
  filter->add_channel(Study_Channel_UNKNOWN);

  filter->add_platform(Study_Platform_PLATFORM_ANDROID);
  filter->add_platform(Study_Platform_PLATFORM_IOS);
  filter->add_platform(Study_Platform_PLATFORM_ANDROID_WEBVIEW);
  filter->add_platform(Study_Platform_PLATFORM_WINDOWS);
  filter->add_platform(Study_Platform_PLATFORM_MAC);
  filter->add_platform(Study_Platform_PLATFORM_LINUX);
  filter->add_platform(Study_Platform_PLATFORM_CHROMEOS);
  filter->add_platform(Study_Platform_PLATFORM_CHROMEOS_LACROS);

  auto* experiment = study->add_experiment();
  experiment->set_name("TestExperiment");
  experiment->set_probability_weight(100);

  auto* layer_member_reference = study->mutable_layer();
  layer_member_reference->set_layer_id(1);
  layer_member_reference->add_layer_member_ids(1);

  return seed;
}

struct CommandLineOutput {
  int exit_code;
  std::string standard_output;
};

CommandLineOutput RunWithCommandLine(const base::CommandLine& command_line) {
  std::string standard_output;
  int exit_code;
  base::GetAppOutputWithExitCode(command_line, &standard_output, &exit_code);
  return {exit_code, standard_output};
}

}  // namespace

class LimitedEntropyRandomizationBrowserTest : public InProcessBrowserTest {
 public:
  LimitedEntropyRandomizationBrowserTest() {
    EnableLimitedEntropyModeForTesting();
    DisableTestingConfig();
  }
  ~LimitedEntropyRandomizationBrowserTest() override = default;
};

// This is a MANUAL test that's only expected to be triggered from
// `LimitedEntropyRandomizationBrowserTestHelper`, which will set up the prefs
// values needed for this test. This test has two purposes: 1) verify that both
// the test study and the synthetic trial are registered, and 2) verify that the
// browser doesn't crash in any of the intermediate steps.
IN_PROC_BROWSER_TEST_F(LimitedEntropyRandomizationBrowserTest,
                       MANUAL_SyntheticTrialAndStudyRegistrationSubTest) {
  EXPECT_TRUE(base::FieldTrialList::TrialExists(kTestStudyName));

  std::vector<ActiveGroupId> synthetic_trials;
  g_browser_process->metrics_service()
      ->GetSyntheticTrialRegistry()
      ->GetSyntheticFieldTrialsOlderThan(base::TimeTicks::Now(),
                                         &synthetic_trials);
  EXPECT_TRUE(
      ContainsTrialName(synthetic_trials, kLimitedEntropySyntheticTrialName));
}

class LimitedEntropyRandomizationBrowserTestHelper : public ::testing::Test {
 public:
  void SetUp() override {
    ::testing::Test::SetUp();
    base::ScopedAllowBlockingForTesting allow_blocking;

    pref_registry_ = base::MakeRefCounted<PrefRegistrySimple>();
    metrics::MetricsService::RegisterPrefs(pref_registry_.get());
    VariationsService::RegisterPrefs(pref_registry_.get());

    // Creates an empty locate state file in preparation for any prefs value
    // needed for the test.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    user_data_dir_ = temp_dir_.GetPath().AppendASCII("user-data-dir");
    ASSERT_TRUE(base::CreateDirectory(user_data_dir_));
    local_state_file_ = user_data_dir_.AppendASCII("Local State");
    base::File local_state_file(
        local_state_file_, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    ASSERT_TRUE(local_state_file.IsValid())
        << "Failed to create local state file: "
        << base::File::ErrorToString(local_state_file.error_details());
  }

  base::CommandLine GetArgsForBrowserTest() {
    base::CommandLine args =
        base::CommandLine(base::CommandLine::ForCurrentProcess()->GetProgram());

    args.AppendSwitch(switches::kAcceptEmptySeedSignatureForTesting);
    args.AppendSwitchASCII(switches::kFakeVariationsChannel, "canary");

    args.AppendSwitchASCII(base::kGTestFilterFlag,
                           "LimitedEntropyRandomizationBrowserTest."
                           "MANUAL_SyntheticTrialAndStudyRegistrationSubTest");
    args.AppendSwitch(::switches::kRunManualTestsFlag);
    args.AppendSwitch(::switches::kSingleProcessTests);

    // Make sure the sub test loads its prefs values from the same disk location
    // in `LimitedEntropyRandomizationBrowserTestHelper`.
    args.AppendSwitchPath(::switches::kUserDataDir, user_data_dir());

    args.AppendSwitchASCII("gtest_color", "no");

    return args;
  }

  const base::FilePath& user_data_dir() const { return user_data_dir_; }
  const base::FilePath& local_state_file() const { return local_state_file_; }

  // Instantiates a pref store backed by a file at the given `path`.
  std::unique_ptr<PrefService> LoadLocalState(const base::FilePath& path) {
    PrefServiceFactory pref_service_factory;
    pref_service_factory.set_async(false);
    pref_service_factory.SetUserPrefsFile(
        path, task_environment_.GetMainThreadTaskRunner().get());
    return pref_service_factory.Create(pref_registry_);
  }

 private:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<PrefRegistrySimple> pref_registry_;
  base::ScopedTempDir temp_dir_;
  base::FilePath user_data_dir_;
  base::FilePath local_state_file_;
};

TEST_F(LimitedEntropyRandomizationBrowserTestHelper,
       SyntheticTrialAndStudyRegistration) {
  std::unique_ptr<PrefService> local_state = LoadLocalState(local_state_file());
  auto args = GetArgsForBrowserTest();

  // Set up the test seed that includes a layer with `EntropyMode.LIMITED`. An
  // empty signature is given since the sub test will be start with
  // `switches::kAcceptEmptySeedSignatureForTesting`.
  local_state->SetString(prefs::kVariationsSeedSignature, "");
  local_state->SetString(
      prefs::kVariationsCompressedSeed,
      GZipAndB64EncodeToHexString(CreateTestSeedWithLimitedEntropyLayer()));

  // This will pick the "Enabled" group in the synthetic trial.
  local_state->SetUint64(prefs::kVariationsLimitedEntropySyntheticTrialSeed,
                         10);

  // Block until the above prefs values are committed to disk. The sub test will
  // spin up a separate browser process which will load its prefs values from
  // the same disk location.
  base::RunLoop loop;
  local_state->CommitPendingWrite(
      base::BindOnce([](base::RunLoop* loop) { loop->Quit(); }, &loop));
  loop.Run();

  // The test verifications are in the sub test,
  // i.e. "MANUAL_SyntheticTrialAndStudyRegistrationSubTest", which is triggered
  // from the command line run. If any of the verifications fail, the command
  // line run will fail and the details of the error will be printed through
  // `output.standard_output`.
  auto output = RunWithCommandLine(args);
  EXPECT_EQ(0, output.exit_code) << "Test failed: " << output.standard_output;
}

}  // namespace variations
