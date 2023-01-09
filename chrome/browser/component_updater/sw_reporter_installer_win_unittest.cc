// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/sw_reporter_installer_win.h"

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/values_util.h"
#include "base/metrics/field_trial.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_reg_util_win.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/version.h"
#include "base/win/registry.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/reporter_runner_win.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/srt_field_trial_win.h"
#include "components/chrome_cleaner/public/constants/constants.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "components/component_updater/pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace component_updater {

namespace {

constexpr char kErrorHistogramName[] = "SoftwareReporter.ConfigurationErrors";
constexpr char kMissingTag[] = "missing_tag";

using safe_browsing::SwReporterInvocation;
using safe_browsing::SwReporterInvocationSequence;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::Contains;
using ::testing::ReturnRef;

using Events = update_client::UpdateClient::Observer::Events;

}  // namespace

class SwReporterInstallerTest : public ::testing::Test {
 public:
  SwReporterInstallerTest()
      : on_component_ready_callback_(base::BindRepeating(
            &SwReporterInstallerTest::SwReporterComponentReady,
            base::Unretained(this))),
        default_version_("1.2.3"),
        default_path_(L"C:\\full\\path\\to\\download") {
    RegisterPrefsForSwReporter(test_prefs_.registry());
  }

  SwReporterInstallerTest(const SwReporterInstallerTest&) = delete;
  SwReporterInstallerTest& operator=(const SwReporterInstallerTest&) = delete;

 protected:
  void SwReporterComponentReady(const std::string& prompt_seed,
                                SwReporterInvocationSequence&& invocations) {
    ASSERT_TRUE(extracted_invocations_.container().empty())
        << "SwReporterComponentReady called more than once.";
    extracted_prompt_seed_ = prompt_seed;
    extracted_invocations_ = std::move(invocations);
  }

  base::FilePath MakeTestFilePath(const base::FilePath& path) const {
    return path.Append(L"software_reporter_tool.exe");
  }

  void CreateFeatureWithoutTag() {
    base::FieldTrialParams params;
    CreateFeatureWithParams(params);
  }

  void CreateFeatureWithTag(const std::string& tag) {
    base::FieldTrialParams params{{"reporter_omaha_tag", tag}};
    CreateFeatureWithParams(params);
  }

  void CreateFeatureWithParams(const base::FieldTrialParams& params) {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        safe_browsing::kChromeCleanupDistributionFeature, params);
  }

  void DisableFeature() {
    scoped_feature_list_.InitAndDisableFeature(
        safe_browsing::kChromeCleanupDistributionFeature);
  }

  void SetReporterCohortPrefs(const std::string& name,
                              base::Time selection_time) {
    test_prefs_.SetUserPref(prefs::kSwReporterCohort, base::Value(name));
    test_prefs_.SetUserPref(prefs::kSwReporterCohortSelectionTime,
                            base::TimeToValue(selection_time));
  }

  // Expects the "tag" attribute will include any of the values in "tags".
  // Returns the value of the attribute or the empty string if not found.
  std::string ExpectAttributesWithTagIn(const SwReporterInstallerPolicy& policy,
                                        const std::vector<std::string>& tags) {
    update_client::InstallerAttributes attributes =
        policy.GetInstallerAttributes();
    EXPECT_EQ(1U, attributes.size());
    std::string tag = attributes["tag"];
    EXPECT_THAT(tags, Contains(tag));
    return tag;
  }

  // Expects the "tag" attribute will be `tag`.
  void ExpectAttributesWithTag(const SwReporterInstallerPolicy& policy,
                               const std::string& tag) {
    ExpectAttributesWithTagIn(policy, {tag});
  }

  void ExpectEmptyAttributes(const SwReporterInstallerPolicy& policy) const {
    update_client::InstallerAttributes attributes =
        policy.GetInstallerAttributes();
    EXPECT_TRUE(attributes.empty());
  }

  // Expects that the SwReporter was launched exactly once, with a session-id
  // switch and the given |expected_prompt_seed|.
  void ExpectDefaultInvocation(const std::string& expected_prompt_seed) const {
    EXPECT_EQ(default_version_, extracted_invocations_.version());
    ASSERT_EQ(1U, extracted_invocations_.container().size());

    const SwReporterInvocation& invocation =
        extracted_invocations_.container().front();
    EXPECT_EQ(MakeTestFilePath(default_path_),
              invocation.command_line().GetProgram());
    EXPECT_EQ(1U, invocation.command_line().GetSwitches().size());
    EXPECT_FALSE(invocation.command_line()
                     .GetSwitchValueASCII(chrome_cleaner::kSessionIdSwitch)
                     .empty());
    EXPECT_TRUE(invocation.command_line().GetArgs().empty());
    EXPECT_TRUE(invocation.suffix().empty());
    EXPECT_EQ(SwReporterInvocation::BEHAVIOURS_ENABLED_BY_DEFAULT,
              invocation.supported_behaviours());
    EXPECT_EQ(extracted_prompt_seed_, expected_prompt_seed);
  }

  // Expects that the SwReporter was launched exactly once, with the given
  // |expected_suffix| and |expected_prompt_seed|, a session-id, and optionally
  // one |expected_additional_argument| on the command-line.
  // (|expected_additional_argument| mainly exists to test that arguments are
  // included at all, so there is no need to test for combinations of multiple
  // arguments and switches in this function.)
  void ExpectInvocationFromManifest(
      const std::string& expected_suffix,
      const std::string& expected_prompt_seed,
      const std::wstring& expected_additional_argument) {
    EXPECT_EQ(default_version_, extracted_invocations_.version());
    ASSERT_EQ(1U, extracted_invocations_.container().size());

    const SwReporterInvocation& invocation =
        extracted_invocations_.container().front();
    EXPECT_EQ(MakeTestFilePath(default_path_),
              invocation.command_line().GetProgram());
    EXPECT_FALSE(invocation.command_line()
                     .GetSwitchValueASCII(chrome_cleaner::kSessionIdSwitch)
                     .empty());

    if (expected_suffix.empty()) {
      EXPECT_EQ(1U, invocation.command_line().GetSwitches().size());
      EXPECT_TRUE(invocation.suffix().empty());
    } else {
      EXPECT_EQ(2U, invocation.command_line().GetSwitches().size());
      EXPECT_EQ(expected_suffix, invocation.command_line().GetSwitchValueASCII(
                                     chrome_cleaner::kRegistrySuffixSwitch));
      EXPECT_EQ(expected_suffix, invocation.suffix());
    }

    if (expected_additional_argument.empty()) {
      EXPECT_TRUE(invocation.command_line().GetArgs().empty());
    } else {
      EXPECT_EQ(1U, invocation.command_line().GetArgs().size());
      EXPECT_EQ(expected_additional_argument,
                invocation.command_line().GetArgs()[0]);
    }

    EXPECT_EQ(0U, invocation.supported_behaviours());
    histograms_.ExpectTotalCount(kErrorHistogramName, 0);

    EXPECT_EQ(extracted_prompt_seed_, expected_prompt_seed);
  }

  // Expects that the SwReporter was launched with the given |expected_suffix|,
  // |expected_engine|, and |expected_behaviours|, as part of a series of
  // multiple invocations.
  void ConsumeAndCheckExperimentFromManifestInSeries(
      const std::string& expected_suffix,
      const std::string& expected_engine,
      SwReporterInvocation::Behaviours expected_behaviours,
      std::string* out_session_id) {
    SCOPED_TRACE("Invocation with suffix " + expected_suffix);
    SwReporterInvocation invocation =
        extracted_invocations_.container().front();
    extracted_invocations_.mutable_container().pop();
    EXPECT_EQ(MakeTestFilePath(default_path_),
              invocation.command_line().GetProgram());
    // There should be one switch added from the manifest, plus registry-suffix
    // and session-id added automatically.
    EXPECT_EQ(3U, invocation.command_line().GetSwitches().size());
    EXPECT_EQ(expected_engine,
              invocation.command_line().GetSwitchValueASCII("engine"));
    EXPECT_EQ(expected_suffix, invocation.command_line().GetSwitchValueASCII(
                                   chrome_cleaner::kRegistrySuffixSwitch));
    *out_session_id = invocation.command_line().GetSwitchValueASCII(
        chrome_cleaner::kSessionIdSwitch);
    EXPECT_FALSE(out_session_id->empty());
    ASSERT_TRUE(invocation.command_line().GetArgs().empty());
    EXPECT_EQ(expected_suffix, invocation.suffix());
    EXPECT_EQ(expected_behaviours, invocation.supported_behaviours());
  }

  void ExpectLaunchError(SoftwareReporterConfigurationError error) {
    // The SwReporter should not be launched, and an error should be logged.
    EXPECT_TRUE(extracted_invocations_.container().empty());
    EXPECT_TRUE(extracted_prompt_seed_.empty());
    histograms_.ExpectUniqueSample(kErrorHistogramName, error, 1);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histograms_;
  TestingPrefServiceSimple test_prefs_;

  // |ComponentReady| asserts that it is run on the UI thread, so we must
  // create test threads before calling it.
  content::BrowserTaskEnvironment task_environment_;

  // Bound callback to the |SwReporterComponentReady| method.
  OnComponentReadyCallback on_component_ready_callback_;

  // Default parameters for |ComponentReady|.
  base::Version default_version_;
  base::FilePath default_path_;

  // Invocations captured by |SwReporterComponentReady|.
  SwReporterInvocationSequence extracted_invocations_;

  // Prompt seed captured by |SwReporterComponentReady|.
  std::string extracted_prompt_seed_;
};

TEST_F(SwReporterInstallerTest, MissingManifest) {
  SwReporterInstallerPolicy policy(&test_prefs_, on_component_ready_callback_);
  policy.ComponentReady(default_version_, default_path_, base::Value::Dict());
  ExpectLaunchError(kMissingPromptSeed);
}

TEST_F(SwReporterInstallerTest, MissingTagRandomCohort) {
  SwReporterInstallerPolicy policy(&test_prefs_, on_component_ready_callback_);
  CreateFeatureWithoutTag();
  std::string tag = ExpectAttributesWithTagIn(policy, {"canary", "stable"});
  histograms_.ExpectUniqueSample(kErrorHistogramName, kBadTag, 0);
  // Randomly assigned tag should be written to prefs.
  EXPECT_EQ(test_prefs_.GetString(prefs::kSwReporterCohort), tag);
}

TEST_F(SwReporterInstallerTest, InvalidTag) {
  SwReporterInstallerPolicy policy(&test_prefs_, on_component_ready_callback_);
  CreateFeatureWithTag("tag with invalid whitespace chars");
  ExpectAttributesWithTag(policy, kMissingTag);
  histograms_.ExpectUniqueSample(kErrorHistogramName, kBadTag, 1);
  // Invalid tag should NOT be written to prefs.
  EXPECT_TRUE(test_prefs_.GetString(prefs::kSwReporterCohort).empty());
}

TEST_F(SwReporterInstallerTest, TagTooLong) {
  SwReporterInstallerPolicy policy(&test_prefs_, on_component_ready_callback_);
  std::string tag_too_long(500, 'x');
  CreateFeatureWithTag(tag_too_long);
  ExpectAttributesWithTag(policy, kMissingTag);
  histograms_.ExpectUniqueSample(kErrorHistogramName, kBadTag, 1);
  // Invalid tag should NOT be written to prefs.
  EXPECT_TRUE(test_prefs_.GetString(prefs::kSwReporterCohort).empty());
}

TEST_F(SwReporterInstallerTest, EmptyTagRandomCohort) {
  SwReporterInstallerPolicy policy(&test_prefs_, on_component_ready_callback_);
  CreateFeatureWithTag("");
  std::string tag = ExpectAttributesWithTagIn(policy, {"canary", "stable"});
  histograms_.ExpectUniqueSample(kErrorHistogramName, kBadTag, 0);
  // Randomly assigned tag should be written to prefs.
  EXPECT_EQ(test_prefs_.GetString(prefs::kSwReporterCohort), tag);
}

TEST_F(SwReporterInstallerTest, ValidTag) {
  SwReporterInstallerPolicy policy(&test_prefs_, on_component_ready_callback_);
  CreateFeatureWithTag("experiment_tag");
  ExpectAttributesWithTag(policy, "experiment_tag");
  histograms_.ExpectUniqueSample(kErrorHistogramName, kBadTag, 0);
  // Tag from feature param should NOT be written to prefs.
  EXPECT_TRUE(test_prefs_.GetString(prefs::kSwReporterCohort).empty());
}

TEST_F(SwReporterInstallerTest, TagFeatureDisabledRandomCohort) {
  SwReporterInstallerPolicy policy(&test_prefs_, on_component_ready_callback_);
  DisableFeature();
  std::string tag = ExpectAttributesWithTagIn(policy, {"canary", "stable"});
  histograms_.ExpectUniqueSample(kErrorHistogramName, kBadTag, 0);
  // Randomly assigned tag should be written to prefs.
  EXPECT_EQ(test_prefs_.GetString(prefs::kSwReporterCohort), tag);
}

TEST_F(SwReporterInstallerTest, TagFromCohortPref) {
  SwReporterInstallerPolicy policy(&test_prefs_, on_component_ready_callback_);
  SetReporterCohortPrefs("canary", base::Time::Now());
  // Make sure if the policy generates a random value, the result will be
  // distinguishable from the cohort.
  policy.SetRandomReporterCohortForTesting("invalid");
  DisableFeature();
  ExpectAttributesWithTag(policy, "canary");
}

TEST_F(SwReporterInstallerTest, OldCohortPrefReshuffled) {
  SwReporterInstallerPolicy policy(&test_prefs_, on_component_ready_callback_);
  SetReporterCohortPrefs("canary", base::Time::Now() - base::Days(40));
  // Expect "canary" to be ignored because the pref was set >30 days ago. Force
  // the random result to be "stable" to distinguish it from the pref.
  policy.SetRandomReporterCohortForTesting("stable");
  DisableFeature();
  ExpectAttributesWithTag(policy, "stable");
  EXPECT_EQ(test_prefs_.GetString(prefs::kSwReporterCohort), "stable");
}

TEST_F(SwReporterInstallerTest, TooNewCohortPrefReshuffled) {
  SwReporterInstallerPolicy policy(&test_prefs_, on_component_ready_callback_);
  SetReporterCohortPrefs("stable", base::Time::Now() + base::Days(2));
  // Expect "stable" to be ignored because the pref was set >1 day in the
  // future. Force the random result to be "canary" to distinguish it from the
  // pref.
  policy.SetRandomReporterCohortForTesting("canary");
  DisableFeature();
  ExpectAttributesWithTag(policy, "canary");
  EXPECT_EQ(test_prefs_.GetString(prefs::kSwReporterCohort), "canary");
}

TEST_F(SwReporterInstallerTest, CohortPrefWithoutTimeReshuffled) {
  SwReporterInstallerPolicy policy(&test_prefs_, on_component_ready_callback_);
  test_prefs_.SetUserPref(prefs::kSwReporterCohort, base::Value("stable"));
  // Expect "stable" to be ignored because the kSwReporterCohortSelectionTime
  // pref is missing. Force the random result to be "canary" to distinguish it
  // from the kSwReporterCohort pref result.
  policy.SetRandomReporterCohortForTesting("canary");
  DisableFeature();
  ExpectAttributesWithTag(policy, "canary");
  EXPECT_EQ(test_prefs_.GetString(prefs::kSwReporterCohort), "canary");
}

TEST_F(SwReporterInstallerTest, InvalidCohortPrefIgnored) {
  SwReporterInstallerPolicy policy(&test_prefs_, on_component_ready_callback_);
  SetReporterCohortPrefs("unknown", base::Time::Now());
  DisableFeature();
  std::string tag = ExpectAttributesWithTagIn(policy, {"canary", "stable"});
  // Randomly assigned tag should be written to prefs.
  EXPECT_EQ(test_prefs_.GetString(prefs::kSwReporterCohort), tag);
}

TEST_F(SwReporterInstallerTest, SingleInvocation) {
  SwReporterInstallerPolicy policy(&test_prefs_, on_component_ready_callback_);

  static constexpr char kTestManifest[] = R"json(
      {
        "launch_params": [
          {
            "arguments": ["--engine=experimental", "random argument"],
            "suffix": "TestSuffix",
            "prompt": false
          }
        ],
        "prompt_seed": "20220421SEED123"
      })json";
  policy.ComponentReady(default_version_, default_path_,
                        (*base::JSONReader::Read(kTestManifest)).TakeDict());

  // The SwReporter should be launched once with the given arguments.
  EXPECT_EQ(default_version_, extracted_invocations_.version());
  ASSERT_EQ(1U, extracted_invocations_.container().size());

  const SwReporterInvocation& invocation =
      extracted_invocations_.container().front();
  EXPECT_EQ(MakeTestFilePath(default_path_),
            invocation.command_line().GetProgram());
  EXPECT_EQ(3U, invocation.command_line().GetSwitches().size());
  EXPECT_EQ("experimental",
            invocation.command_line().GetSwitchValueASCII("engine"));
  EXPECT_EQ("TestSuffix", invocation.command_line().GetSwitchValueASCII(
                              chrome_cleaner::kRegistrySuffixSwitch));
  EXPECT_FALSE(invocation.command_line()
                   .GetSwitchValueASCII(chrome_cleaner::kSessionIdSwitch)
                   .empty());
  ASSERT_EQ(1U, invocation.command_line().GetArgs().size());
  EXPECT_EQ(L"random argument", invocation.command_line().GetArgs()[0]);
  EXPECT_EQ("TestSuffix", invocation.suffix());
  EXPECT_EQ(0U, invocation.supported_behaviours());

  EXPECT_EQ("20220421SEED123", extracted_prompt_seed_);

  histograms_.ExpectTotalCount(kErrorHistogramName, 0);
}

TEST_F(SwReporterInstallerTest, MultipleInvocations) {
  SwReporterInstallerPolicy policy(&test_prefs_, on_component_ready_callback_);

  static constexpr char kTestManifest[] = R"json(
      {
        "launch_params": [
          {
            "arguments": ["--engine=experimental"],
            "suffix": "TestSuffix",
            "prompt": false,
            "allow-reporter-logs": true
          },
          {
            "arguments": ["--engine=second"],
            "suffix": "SecondSuffix",
            "prompt": true,
            "allow-reporter-logs": false
          },
          {
            "arguments": ["--engine=third"],
            "suffix": "ThirdSuffix"
          },
          {
            "arguments": ["--engine=fourth"],
            "suffix": "FourthSuffix",
            "prompt": true,
            "allow-reporter-logs": true
          }
        ],
        "prompt_seed": "20220421SEED123"
      })json";
  policy.ComponentReady(default_version_, default_path_,
                        (*base::JSONReader::Read(kTestManifest)).TakeDict());

  // The SwReporter should be launched four times with the given arguments.
  EXPECT_EQ(default_version_, extracted_invocations_.version());
  ASSERT_EQ(4U, extracted_invocations_.container().size());
  std::string out_session_id;
  ConsumeAndCheckExperimentFromManifestInSeries("TestSuffix", "experimental",
                                                /*supported_behaviours=*/0,
                                                &out_session_id);

  const std::string first_session_id(out_session_id);

  ConsumeAndCheckExperimentFromManifestInSeries(
      "SecondSuffix", "second", SwReporterInvocation::BEHAVIOUR_TRIGGER_PROMPT,
      &out_session_id);
  EXPECT_EQ(first_session_id, out_session_id);

  ConsumeAndCheckExperimentFromManifestInSeries("ThirdSuffix", "third", 0U,
                                                &out_session_id);
  EXPECT_EQ(first_session_id, out_session_id);

  ConsumeAndCheckExperimentFromManifestInSeries(
      "FourthSuffix", "fourth", SwReporterInvocation::BEHAVIOUR_TRIGGER_PROMPT,
      &out_session_id);
  EXPECT_EQ(first_session_id, out_session_id);

  EXPECT_EQ("20220421SEED123", extracted_prompt_seed_);

  histograms_.ExpectTotalCount(kErrorHistogramName, 0);
}

TEST_F(SwReporterInstallerTest, MissingSuffix) {
  SwReporterInstallerPolicy policy(&test_prefs_, on_component_ready_callback_);

  static constexpr char kTestManifest[] = R"json(
      {
        "launch_params": [
          {
            "arguments": ["random argument"]
          }
        ],
        "prompt_seed": "20220421SEED123"
      })json";
  policy.ComponentReady(default_version_, default_path_,
                        (*base::JSONReader::Read(kTestManifest)).TakeDict());
  ExpectLaunchError(kBadParams);
}

TEST_F(SwReporterInstallerTest, EmptySuffix) {
  SwReporterInstallerPolicy policy(&test_prefs_, on_component_ready_callback_);

  static constexpr char kTestManifest[] = R"json(
      {
        "launch_params": [
          {
            "suffix": "",
            "arguments": ["random argument"]
          }
        ],
        "prompt_seed": "20220421SEED123"
      })json";
  policy.ComponentReady(default_version_, default_path_,
                        (*base::JSONReader::Read(kTestManifest)).TakeDict());
  ExpectInvocationFromManifest("", "20220421SEED123", L"random argument");
}

TEST_F(SwReporterInstallerTest, MissingSuffixAndArgs) {
  SwReporterInstallerPolicy policy(&test_prefs_, on_component_ready_callback_);

  static constexpr char kTestManifest[] = R"json(
      {
        "launch_params": [
          {
          }
        ],
        "prompt_seed": "20220421SEED123"
      })json";
  policy.ComponentReady(default_version_, default_path_,
                        (*base::JSONReader::Read(kTestManifest)).TakeDict());
  ExpectLaunchError(kBadParams);
}

TEST_F(SwReporterInstallerTest, EmptySuffixAndArgs) {
  SwReporterInstallerPolicy policy(&test_prefs_, on_component_ready_callback_);

  static constexpr char kTestManifest[] = R"json(
      {
        "launch_params": [
          {
            "suffix": "",
            "arguments": []
          }
        ],
        "prompt_seed": "20220421SEED123"
      })json";
  policy.ComponentReady(default_version_, default_path_,
                        (*base::JSONReader::Read(kTestManifest)).TakeDict());
  ExpectInvocationFromManifest("", "20220421SEED123", {});
}

TEST_F(SwReporterInstallerTest, EmptySuffixAndArgsWithEmptyString) {
  SwReporterInstallerPolicy policy(&test_prefs_, on_component_ready_callback_);

  static constexpr char kTestManifest[] = R"json(
      {
        "launch_params": [
          {
            "suffix": "",
            "arguments": [""]
          }
        ],
        "prompt_seed": "20220421SEED123"
      })json";
  policy.ComponentReady(default_version_, default_path_,
                        (*base::JSONReader::Read(kTestManifest)).TakeDict());
  ExpectInvocationFromManifest("", "20220421SEED123", {});
}

TEST_F(SwReporterInstallerTest, MissingArguments) {
  SwReporterInstallerPolicy policy(&test_prefs_, on_component_ready_callback_);

  static constexpr char kTestManifest[] = R"json(
      {
        "launch_params": [
          {
            "suffix": "TestSuffix"
          }
        ],
        "prompt_seed": "20220421SEED123"
      })json";
  policy.ComponentReady(default_version_, default_path_,
                        (*base::JSONReader::Read(kTestManifest)).TakeDict());
  ExpectLaunchError(kBadParams);
}

TEST_F(SwReporterInstallerTest, EmptyArguments) {
  SwReporterInstallerPolicy policy(&test_prefs_, on_component_ready_callback_);

  static constexpr char kTestManifest[] = R"json(
      {
        "launch_params": [
          {
            "suffix": "TestSuffix",
            "arguments": []
          }
        ],
        "prompt_seed": "20220421SEED123"
      })json";
  policy.ComponentReady(default_version_, default_path_,
                        (*base::JSONReader::Read(kTestManifest)).TakeDict());
  ExpectInvocationFromManifest("TestSuffix", "20220421SEED123", {});
}

TEST_F(SwReporterInstallerTest, EmptyArgumentsWithEmptyString) {
  SwReporterInstallerPolicy policy(&test_prefs_, on_component_ready_callback_);

  static constexpr char kTestManifest[] = R"json(
      {
        "launch_params": [
          {
            "suffix": "TestSuffix",
            "arguments": [""]
          }
        ],
        "prompt_seed": "20220421SEED123"
      })json";
  policy.ComponentReady(default_version_, default_path_,
                        (*base::JSONReader::Read(kTestManifest)).TakeDict());
  ExpectInvocationFromManifest("TestSuffix", "20220421SEED123", {});
}

TEST_F(SwReporterInstallerTest, EmptyManifest) {
  SwReporterInstallerPolicy policy(&test_prefs_, on_component_ready_callback_);

  static constexpr char kTestManifest[] = "{}";
  policy.ComponentReady(default_version_, default_path_,
                        (*base::JSONReader::Read(kTestManifest)).TakeDict());
  ExpectLaunchError(kMissingPromptSeed);
}

TEST_F(SwReporterInstallerTest, MissingLaunchParams) {
  SwReporterInstallerPolicy policy(&test_prefs_, on_component_ready_callback_);

  static constexpr char kTestManifest[] = R"json(
      {
        "prompt_seed": "20220421SEED123"
      })json";
  policy.ComponentReady(default_version_, default_path_,
                        (*base::JSONReader::Read(kTestManifest)).TakeDict());
  ExpectDefaultInvocation("20220421SEED123");
}

TEST_F(SwReporterInstallerTest, EmptyLaunchParams) {
  SwReporterInstallerPolicy policy(&test_prefs_, on_component_ready_callback_);

  static constexpr char kTestManifest[] = R"json(
      {
        "launch_params": [],
        "prompt_seed": "20220421SEED123"
      })json";
  policy.ComponentReady(default_version_, default_path_,
                        (*base::JSONReader::Read(kTestManifest)).TakeDict());
  ExpectDefaultInvocation("20220421SEED123");
}

TEST_F(SwReporterInstallerTest, MissingPromptSeed) {
  SwReporterInstallerPolicy policy(&test_prefs_, on_component_ready_callback_);

  static constexpr char kTestManifest[] = R"json(
      {
        "launch_params": [
          {
            "arguments": ["--engine=experimental"],
            "suffix": "TestSuffix"
          }
        ]
      })json";
  policy.ComponentReady(default_version_, default_path_,
                        (*base::JSONReader::Read(kTestManifest)).TakeDict());
  ExpectLaunchError(kMissingPromptSeed);
}

TEST_F(SwReporterInstallerTest, BadSuffix) {
  SwReporterInstallerPolicy policy(&test_prefs_, on_component_ready_callback_);

  static constexpr char kTestManifest[] = R"json(
      {
        "launch_params": [
          {
            "arguments": ["--engine=experimental"],
            "suffix": "invalid whitespace characters"
          }
        ],
        "prompt_seed": "20220421SEED123"
      })json";
  policy.ComponentReady(default_version_, default_path_,
                        (*base::JSONReader::Read(kTestManifest)).TakeDict());
  ExpectLaunchError(kBadParams);
}

TEST_F(SwReporterInstallerTest, SuffixTooLong) {
  SwReporterInstallerPolicy policy(&test_prefs_, on_component_ready_callback_);

  static constexpr char kTestManifest[] = R"json(
      {
        "launch_params": [
          {
            "arguments": ["--engine=experimental"],
            "suffix": "%s"
          }
        ],
        "prompt_seed": "20220421SEED123"
      })json";
  std::string suffix_too_long(500, 'x');
  std::string manifest =
      base::StringPrintf(kTestManifest, suffix_too_long.c_str());
  policy.ComponentReady(default_version_, default_path_,
                        (*base::JSONReader::Read(manifest)).TakeDict());
  ExpectLaunchError(kBadParams);
}

TEST_F(SwReporterInstallerTest, BadTypesInManifest_ArgumentsIsNotAList) {
  SwReporterInstallerPolicy policy(&test_prefs_, on_component_ready_callback_);

  // This has a string instead of a list for "arguments".
  static constexpr char kTestManifest[] = R"json(
      {
        "launch_params": [
          {
            "arguments": "--engine=experimental",
            "suffix": "TestSuffix"
          }
        ],
        "prompt_seed": "20220421SEED123"
      })json";
  policy.ComponentReady(default_version_, default_path_,
                        (*base::JSONReader::Read(kTestManifest)).TakeDict());
  ExpectLaunchError(kBadParams);
}

TEST_F(SwReporterInstallerTest, BadTypesInManifest_InvocationParamsIsNotAList) {
  SwReporterInstallerPolicy policy(&test_prefs_, on_component_ready_callback_);

  // This has the invocation parameters as direct children of "launch_params",
  // instead of using a list.
  static constexpr char kTestManifest[] = R"json(
      {
        "launch_params": {
            "arguments": ["--engine=experimental"],
            "suffix": "TestSuffix"
        },
        "prompt_seed": "20220421SEED123"
      })json";
  policy.ComponentReady(default_version_, default_path_,
                        (*base::JSONReader::Read(kTestManifest)).TakeDict());
  ExpectLaunchError(kBadParams);
}

TEST_F(SwReporterInstallerTest, BadTypesInManifest_SuffixIsAList) {
  SwReporterInstallerPolicy policy(&test_prefs_, on_component_ready_callback_);

  // This has a list for suffix as well as for arguments.
  static constexpr char kTestManifest[] = R"json(
      {
        "launch_params": [
          {
            "arguments": ["--engine=experimental"],
            "suffix": ["TestSuffix"]
          }
        ],
        "prompt_seed": "20220421SEED123"
      })json";
  policy.ComponentReady(default_version_, default_path_,
                        (*base::JSONReader::Read(kTestManifest)).TakeDict());
  ExpectLaunchError(kBadParams);
}

TEST_F(SwReporterInstallerTest, BadTypesInManifest_PromptIsNotABoolean) {
  SwReporterInstallerPolicy policy(&test_prefs_, on_component_ready_callback_);

  // This has an int instead of a bool for prompt.
  static constexpr char kTestManifest[] = R"json(
      {
        "launch_params": [
          {
            "arguments": ["--engine=experimental"],
            "suffix": "TestSuffix",
            "prompt": 1
          }
        ],
        "prompt_seed": "20220421SEED123"
      })json";
  policy.ComponentReady(default_version_, default_path_,
                        (*base::JSONReader::Read(kTestManifest)).TakeDict());
  ExpectLaunchError(kBadParams);
}

TEST_F(SwReporterInstallerTest, BadTypesInManifest_LaunchParamsIsScalar) {
  SwReporterInstallerPolicy policy(&test_prefs_, on_component_ready_callback_);

  static constexpr char kTestManifest[] = R"json(
      {
        "launch_params": 0,
        "prompt_seed": "20220421SEED123"
      })json";
  policy.ComponentReady(default_version_, default_path_,
                        (*base::JSONReader::Read(kTestManifest)).TakeDict());
  ExpectLaunchError(kBadParams);
}

TEST_F(SwReporterInstallerTest, BadTypesInManifest_LaunchParamsIsDict) {
  SwReporterInstallerPolicy policy(&test_prefs_, on_component_ready_callback_);

  static constexpr char kTestManifest[] = R"json(
      {
        "launch_params": {},
        "prompt_seed": "20220421SEED123"
      })json";
  policy.ComponentReady(default_version_, default_path_,
                        (*base::JSONReader::Read(kTestManifest)).TakeDict());
  ExpectLaunchError(kBadParams);
}

TEST_F(SwReporterInstallerTest, BadTypesInManifest_PromptSeedIsList) {
  SwReporterInstallerPolicy policy(&test_prefs_, on_component_ready_callback_);

  static constexpr char kTestManifest[] = R"json(
      {
        "launch_params": [],
        "prompt_seed": ["20220421SEED123"]
      })json";
  policy.ComponentReady(default_version_, default_path_,
                        (*base::JSONReader::Read(kTestManifest)).TakeDict());
  ExpectLaunchError(kMissingPromptSeed);
}

TEST_F(SwReporterInstallerTest, BadTypesInManifest_PromptSeedIsInt) {
  SwReporterInstallerPolicy policy(&test_prefs_, on_component_ready_callback_);

  static constexpr char kTestManifest[] = R"json(
      {
        "launch_params": [],
        "prompt_seed": 20220421
      })json";
  policy.ComponentReady(default_version_, default_path_,
                        (*base::JSONReader::Read(kTestManifest)).TakeDict());
  ExpectLaunchError(kMissingPromptSeed);
}

class SwReporterOnDemandFetcherTest : public ::testing::Test,
                                      public OnDemandUpdater {
 public:
  SwReporterOnDemandFetcherTest() = default;

  SwReporterOnDemandFetcherTest(const SwReporterOnDemandFetcherTest&) = delete;
  SwReporterOnDemandFetcherTest& operator=(
      const SwReporterOnDemandFetcherTest&) = delete;

  void SetUp() override {
    EXPECT_CALL(mock_cus_, AddObserver(_)).Times(1);
    EXPECT_CALL(mock_cus_, GetOnDemandUpdater()).WillOnce(ReturnRef(*this));
    EXPECT_CALL(mock_cus_, RemoveObserver(_)).Times(AtLeast(1));
  }

  void CreateOnDemandFetcherAndVerifyExpectations(bool can_be_updated) {
    component_can_be_updated_ = can_be_updated;

    fetcher_ = std::make_unique<SwReporterOnDemandFetcher>(
        &mock_cus_,
        base::BindOnce(&SwReporterOnDemandFetcherTest::SetErrorCallbackCalled,
                       base::Unretained(this)));
  }

  // OnDemandUpdater implementation:
  void OnDemandUpdate(const std::string& crx_id,
                      Priority priority,
                      Callback callback) override {
    ASSERT_EQ(kSwReporterComponentId, crx_id);
    ASSERT_EQ(Priority::FOREGROUND, priority);
    on_demand_update_called_ = true;

    // |OnDemandUpdate| is called immediately on |SwReporterOnDemandFetcher|
    // creation, before |fetcher_| has been assigned the newly created object.
    // Post a task to guarantee that |fetcher_| is initialized.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            component_can_be_updated_
                ? &SwReporterOnDemandFetcherTest::FireComponentUpdateEvents
                : &SwReporterOnDemandFetcherTest::FireComponentNotUpdatedEvents,
            base::Unretained(this)));
  }

 protected:
  ::testing::StrictMock<MockComponentUpdateService> mock_cus_;
  std::unique_ptr<SwReporterOnDemandFetcher> fetcher_;

  bool on_demand_update_called_ = false;

 private:
  void FireComponentUpdateEvents() {
    fetcher_->OnEvent(Events::COMPONENT_CHECKING_FOR_UPDATES,
                      kSwReporterComponentId);
    fetcher_->OnEvent(Events::COMPONENT_UPDATE_FOUND, kSwReporterComponentId);
    fetcher_->OnEvent(Events::COMPONENT_UPDATE_DOWNLOADING,
                      kSwReporterComponentId);
    fetcher_->OnEvent(Events::COMPONENT_UPDATE_READY, kSwReporterComponentId);
    fetcher_->OnEvent(Events::COMPONENT_UPDATED, kSwReporterComponentId);

    EXPECT_FALSE(error_callback_called_);
  }

  void FireComponentNotUpdatedEvents() {
    fetcher_->OnEvent(Events::COMPONENT_CHECKING_FOR_UPDATES,
                      kSwReporterComponentId);
    fetcher_->OnEvent(Events::COMPONENT_UPDATE_ERROR, kSwReporterComponentId);

    EXPECT_TRUE(error_callback_called_);
  }

  void SetErrorCallbackCalled() { error_callback_called_ = true; }

  bool component_can_be_updated_ = false;
  bool error_callback_called_ = false;
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(SwReporterOnDemandFetcherTest, TestUpdateSuccess) {
  CreateOnDemandFetcherAndVerifyExpectations(true);

  EXPECT_TRUE(on_demand_update_called_);
}

TEST_F(SwReporterOnDemandFetcherTest, TestUpdateFailure) {
  CreateOnDemandFetcherAndVerifyExpectations(false);

  EXPECT_TRUE(on_demand_update_called_);
}

}  // namespace component_updater
