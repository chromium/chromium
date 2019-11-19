// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/sw_reporter_installer_win.h"

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_reg_util_win.h"
#include "base/values.h"
#include "base/version.h"
#include "base/win/registry.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/reporter_runner_win.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/srt_field_trial_win.h"
#include "components/chrome_cleaner/public/constants/constants.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace component_updater {

namespace {

constexpr char kErrorHistogramName[] = "SoftwareReporter.ExperimentErrors";
constexpr char kExperimentTag[] = "experiment_tag";
constexpr char kMissingTag[] = "missing_tag";

using safe_browsing::SwReporterInvocation;
using safe_browsing::SwReporterInvocationSequence;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::ReturnRef;

using Events = update_client::UpdateClient::Observer::Events;

}  // namespace

class SwReporterInstallerTest : public ::testing::Test {
 public:
  SwReporterInstallerTest()
      : on_component_ready_callback_(
            base::Bind(&SwReporterInstallerTest::SwReporterComponentReady,
                       base::Unretained(this))),
        default_version_("1.2.3"),
        default_path_(L"C:\\full\\path\\to\\download") {}

 protected:
  void SwReporterComponentReady(SwReporterInvocationSequence&& invocations) {
    ASSERT_TRUE(extracted_invocations_.container().empty());
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
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        safe_browsing::kChromeCleanupDistributionFeature, params);
  }

  void ExpectAttributesWithTag(const SwReporterInstallerPolicy& policy,
                               const std::string& tag) {
    update_client::InstallerAttributes attributes =
        policy.GetInstallerAttributes();
    EXPECT_EQ(1U, attributes.size());
    EXPECT_EQ(tag, attributes["tag"]);
  }

  void ExpectEmptyAttributes(const SwReporterInstallerPolicy& policy) const {
    update_client::InstallerAttributes attributes =
        policy.GetInstallerAttributes();
    EXPECT_TRUE(attributes.empty());
  }

  // Expects that the SwReporter was launched exactly once, with a session-id
  // switch.
  void ExpectDefaultInvocation() const {
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
  }

  // Expects that the SwReporter was launched exactly once, with the given
  // |expected_suffix|, a session-id, and one |expected_additional_argument| on
  // the command-line.  (|expected_additional_argument| mainly exists to test
  // that arguments are included at all, so there is no need to test for
  // combinations of multiple arguments and switches in this function.)
  void ExpectInvocationFromManifest(
      const std::string& expected_suffix,
      const base::string16& expected_additional_argument) {
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

  void ExpectLaunchError() {
    // The SwReporter should not be launched, and an error should be logged.
    EXPECT_TRUE(extracted_invocations_.container().empty());
    histograms_.ExpectUniqueSample(kErrorHistogramName,
                                   SW_REPORTER_EXPERIMENT_ERROR_BAD_PARAMS, 1);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histograms_;

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

 private:
  DISALLOW_COPY_AND_ASSIGN(SwReporterInstallerTest);
};

TEST_F(SwReporterInstallerTest, MissingManifest) {
  SwReporterInstallerPolicy policy(on_component_ready_callback_);
  ExpectEmptyAttributes(policy);
  policy.ComponentReady(default_version_, default_path_,
                        std::make_unique<base::DictionaryValue>());
  ExpectDefaultInvocation();
}

TEST_F(SwReporterInstallerTest, MissingTag) {
  SwReporterInstallerPolicy policy(on_component_ready_callback_);
  CreateFeatureWithoutTag();
  ExpectAttributesWithTag(policy, kMissingTag);
  histograms_.ExpectUniqueSample(kErrorHistogramName,
                                 SW_REPORTER_EXPERIMENT_ERROR_BAD_TAG, 1);
}

TEST_F(SwReporterInstallerTest, InvalidTag) {
  SwReporterInstallerPolicy policy(on_component_ready_callback_);
  CreateFeatureWithTag("tag with invalid whitespace chars");
  ExpectAttributesWithTag(policy, kMissingTag);
  histograms_.ExpectUniqueSample(kErrorHistogramName,
                                 SW_REPORTER_EXPERIMENT_ERROR_BAD_TAG, 1);
}

TEST_F(SwReporterInstallerTest, TagTooLong) {
  SwReporterInstallerPolicy policy(on_component_ready_callback_);
  std::string tag_too_long(500, 'x');
  CreateFeatureWithTag(tag_too_long);
  ExpectAttributesWithTag(policy, kMissingTag);
  histograms_.ExpectUniqueSample(kErrorHistogramName,
                                 SW_REPORTER_EXPERIMENT_ERROR_BAD_TAG, 1);
}

TEST_F(SwReporterInstallerTest, EmptyTag) {
  SwReporterInstallerPolicy policy(on_component_ready_callback_);
  CreateFeatureWithTag("");
  ExpectAttributesWithTag(policy, kMissingTag);
  histograms_.ExpectUniqueSample(kErrorHistogramName,
                                 SW_REPORTER_EXPERIMENT_ERROR_BAD_TAG, 1);
}

TEST_F(SwReporterInstallerTest, ValidTag) {
  SwReporterInstallerPolicy policy(on_component_ready_callback_);
  CreateFeatureWithTag(kExperimentTag);
  ExpectAttributesWithTag(policy, kExperimentTag);
}

TEST_F(SwReporterInstallerTest, SingleInvocation) {
  SwReporterInstallerPolicy policy(on_component_ready_callback_);

  static constexpr char kTestManifest[] =
      "{\"launch_params\": ["
      "  {"
      "    \"arguments\": [\"--engine=experimental\", \"random argument\"],"
      "    \"suffix\": \"TestSuffix\","
      "    \"prompt\": false"
      "  }"
      "]}";
  policy.ComponentReady(default_version_, default_path_,
                        base::DictionaryValue::From(
                            base::JSONReader::ReadDeprecated(kTestManifest)));

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
  histograms_.ExpectTotalCount(kErrorHistogramName, 0);
}

TEST_F(SwReporterInstallerTest, MultipleInvocations) {
  SwReporterInstallerPolicy policy(on_component_ready_callback_);

  static constexpr char kTestManifest[] =
      "{\"launch_params\": ["
      "  {"
      "    \"arguments\": [\"--engine=experimental\"],"
      "    \"suffix\": \"TestSuffix\","
      "    \"prompt\": false,"
      "    \"allow-reporter-logs\": true"
      "  },"
      "  {"
      "    \"arguments\": [\"--engine=second\"],"
      "    \"suffix\": \"SecondSuffix\","
      "    \"prompt\": true,"
      "    \"allow-reporter-logs\": false"
      "  },"
      "  {"
      "    \"arguments\": [\"--engine=third\"],"
      "    \"suffix\": \"ThirdSuffix\""
      "  },"
      "  {"
      "    \"arguments\": [\"--engine=fourth\"],"
      "    \"suffix\": \"FourthSuffix\","
      "    \"prompt\": true,"
      "    \"allow-reporter-logs\": true"
      "  }"

      "]}";
  policy.ComponentReady(default_version_, default_path_,
                        base::DictionaryValue::From(
                            base::JSONReader::ReadDeprecated(kTestManifest)));

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

  histograms_.ExpectTotalCount(kErrorHistogramName, 0);
}

TEST_F(SwReporterInstallerTest, MissingSuffix) {
  SwReporterInstallerPolicy policy(on_component_ready_callback_);

  static constexpr char kTestManifest[] =
      "{\"launch_params\": ["
      "  {"
      "    \"arguments\": [\"random argument\"]"
      "  }"
      "]}";
  policy.ComponentReady(default_version_, default_path_,
                        base::DictionaryValue::From(
                            base::JSONReader::ReadDeprecated(kTestManifest)));

  ExpectLaunchError();
}

TEST_F(SwReporterInstallerTest, EmptySuffix) {
  SwReporterInstallerPolicy policy(on_component_ready_callback_);

  static constexpr char kTestManifest[] =
      "{\"launch_params\": ["
      "  {"
      "    \"suffix\": \"\","
      "    \"arguments\": [\"random argument\"]"
      "  }"
      "]}";
  policy.ComponentReady(default_version_, default_path_,
                        base::DictionaryValue::From(
                            base::JSONReader::ReadDeprecated(kTestManifest)));

  ExpectInvocationFromManifest("", L"random argument");
}

TEST_F(SwReporterInstallerTest, MissingSuffixAndArgs) {
  SwReporterInstallerPolicy policy(on_component_ready_callback_);

  static constexpr char kTestManifest[] =
      "{\"launch_params\": ["
      "  {"
      "  }"
      "]}";
  policy.ComponentReady(default_version_, default_path_,
                        base::DictionaryValue::From(
                            base::JSONReader::ReadDeprecated(kTestManifest)));

  ExpectLaunchError();
}

TEST_F(SwReporterInstallerTest, EmptySuffixAndArgs) {
  SwReporterInstallerPolicy policy(on_component_ready_callback_);

  static constexpr char kTestManifest[] =
      "{\"launch_params\": ["
      "  {"
      "    \"suffix\": \"\","
      "    \"arguments\": []"
      "  }"
      "]}";
  policy.ComponentReady(default_version_, default_path_,
                        base::DictionaryValue::From(
                            base::JSONReader::ReadDeprecated(kTestManifest)));

  ExpectInvocationFromManifest("", L"");
}

TEST_F(SwReporterInstallerTest, EmptySuffixAndArgsWithEmptyString) {
  SwReporterInstallerPolicy policy(on_component_ready_callback_);

  static constexpr char kTestManifest[] =
      "{\"launch_params\": ["
      "  {"
      "    \"suffix\": \"\","
      "    \"arguments\": [\"\"]"
      "  }"
      "]}";
  policy.ComponentReady(default_version_, default_path_,
                        base::DictionaryValue::From(
                            base::JSONReader::ReadDeprecated(kTestManifest)));

  ExpectInvocationFromManifest("", L"");
}

TEST_F(SwReporterInstallerTest, MissingArguments) {
  SwReporterInstallerPolicy policy(on_component_ready_callback_);

  static constexpr char kTestManifest[] =
      "{\"launch_params\": ["
      "  {"
      "    \"suffix\": \"TestSuffix\""
      "  }"
      "]}";
  policy.ComponentReady(default_version_, default_path_,
                        base::DictionaryValue::From(
                            base::JSONReader::ReadDeprecated(kTestManifest)));

  ExpectLaunchError();
}

TEST_F(SwReporterInstallerTest, EmptyArguments) {
  SwReporterInstallerPolicy policy(on_component_ready_callback_);

  static constexpr char kTestManifest[] =
      "{\"launch_params\": ["
      "  {"
      "    \"suffix\": \"TestSuffix\","
      "    \"arguments\": []"
      "  }"
      "]}";
  policy.ComponentReady(default_version_, default_path_,
                        base::DictionaryValue::From(
                            base::JSONReader::ReadDeprecated(kTestManifest)));

  ExpectInvocationFromManifest("TestSuffix", L"");
}

TEST_F(SwReporterInstallerTest, EmptyArgumentsWithEmptyString) {
  SwReporterInstallerPolicy policy(on_component_ready_callback_);

  static constexpr char kTestManifest[] =
      "{\"launch_params\": ["
      "  {"
      "    \"suffix\": \"TestSuffix\","
      "    \"arguments\": [\"\"]"
      "  }"
      "]}";
  policy.ComponentReady(default_version_, default_path_,
                        base::DictionaryValue::From(
                            base::JSONReader::ReadDeprecated(kTestManifest)));

  ExpectInvocationFromManifest("TestSuffix", L"");
}

TEST_F(SwReporterInstallerTest, EmptyManifest) {
  SwReporterInstallerPolicy policy(on_component_ready_callback_);

  static constexpr char kTestManifest[] = "{}";
  policy.ComponentReady(default_version_, default_path_,
                        base::DictionaryValue::From(
                            base::JSONReader::ReadDeprecated(kTestManifest)));
  ExpectDefaultInvocation();
}

TEST_F(SwReporterInstallerTest, EmptyLaunchParams) {
  SwReporterInstallerPolicy policy(on_component_ready_callback_);

  static constexpr char kTestManifest[] = "{\"launch_params\": []}";
  policy.ComponentReady(default_version_, default_path_,
                        base::DictionaryValue::From(
                            base::JSONReader::ReadDeprecated(kTestManifest)));
  ExpectDefaultInvocation();
}

TEST_F(SwReporterInstallerTest, BadSuffix) {
  SwReporterInstallerPolicy policy(on_component_ready_callback_);

  static constexpr char kTestManifest[] =
      "{\"launch_params\": ["
      "  {"
      "    \"arguments\": [\"--engine=experimental\"],"
      "    \"suffix\": \"invalid whitespace characters\""
      "  }"
      "]}";
  policy.ComponentReady(default_version_, default_path_,
                        base::DictionaryValue::From(
                            base::JSONReader::ReadDeprecated(kTestManifest)));

  // The SwReporter should not be launched, and an error should be logged.
  EXPECT_TRUE(extracted_invocations_.container().empty());
  histograms_.ExpectUniqueSample(kErrorHistogramName,
                                 SW_REPORTER_EXPERIMENT_ERROR_BAD_PARAMS, 1);
}

TEST_F(SwReporterInstallerTest, SuffixTooLong) {
  SwReporterInstallerPolicy policy(on_component_ready_callback_);

  static constexpr char kTestManifest[] =
      "{\"launch_params\": ["
      "  {"
      "    \"arguments\": [\"--engine=experimental\"],"
      "    \"suffix\": \"%s\""
      "  }"
      "]}";
  std::string suffix_too_long(500, 'x');
  std::string manifest =
      base::StringPrintf(kTestManifest, suffix_too_long.c_str());
  policy.ComponentReady(
      default_version_, default_path_,
      base::DictionaryValue::From(base::JSONReader::ReadDeprecated(manifest)));

  // The SwReporter should not be launched, and an error should be logged.
  EXPECT_TRUE(extracted_invocations_.container().empty());
  histograms_.ExpectUniqueSample(kErrorHistogramName,
                                 SW_REPORTER_EXPERIMENT_ERROR_BAD_PARAMS, 1);
}

TEST_F(SwReporterInstallerTest, BadTypesInManifest_ArgumentsIsNotAList) {
  SwReporterInstallerPolicy policy(on_component_ready_callback_);

  // This has a string instead of a list for "arguments".
  static constexpr char kTestManifest[] =
      "{\"launch_params\": ["
      "  {"
      "    \"arguments\": \"--engine=experimental\","
      "    \"suffix\": \"TestSuffix\""
      "  }"
      "]}";
  policy.ComponentReady(default_version_, default_path_,
                        base::DictionaryValue::From(
                            base::JSONReader::ReadDeprecated(kTestManifest)));

  // The SwReporter should not be launched, and an error should be logged.
  EXPECT_TRUE(extracted_invocations_.container().empty());
  histograms_.ExpectUniqueSample(kErrorHistogramName,
                                 SW_REPORTER_EXPERIMENT_ERROR_BAD_PARAMS, 1);
}

TEST_F(SwReporterInstallerTest, BadTypesInManifest_InvocationParamsIsNotAList) {
  SwReporterInstallerPolicy policy(on_component_ready_callback_);

  // This has the invocation parameters as direct children of "launch_params",
  // instead of using a list.
  static constexpr char kTestManifest[] =
      "{\"launch_params\": "
      "  {"
      "    \"arguments\": [\"--engine=experimental\"],"
      "    \"suffix\": \"TestSuffix\""
      "  }"
      "}";
  policy.ComponentReady(default_version_, default_path_,
                        base::DictionaryValue::From(
                            base::JSONReader::ReadDeprecated(kTestManifest)));

  // The SwReporter should not be launched, and an error should be logged.
  EXPECT_TRUE(extracted_invocations_.container().empty());
  histograms_.ExpectUniqueSample(kErrorHistogramName,
                                 SW_REPORTER_EXPERIMENT_ERROR_BAD_PARAMS, 1);
}

TEST_F(SwReporterInstallerTest, BadTypesInManifest_SuffixIsAList) {
  SwReporterInstallerPolicy policy(on_component_ready_callback_);

  // This has a list for suffix as well as for arguments.
  static constexpr char kTestManifest[] =
      "{\"launch_params\": ["
      "  {"
      "    \"arguments\": [\"--engine=experimental\"],"
      "    \"suffix\": [\"TestSuffix\"]"
      "  }"
      "]}";
  policy.ComponentReady(default_version_, default_path_,
                        base::DictionaryValue::From(
                            base::JSONReader::ReadDeprecated(kTestManifest)));

  // The SwReporter should not be launched, and an error should be logged.
  EXPECT_TRUE(extracted_invocations_.container().empty());
  histograms_.ExpectUniqueSample(kErrorHistogramName,
                                 SW_REPORTER_EXPERIMENT_ERROR_BAD_PARAMS, 1);
}

TEST_F(SwReporterInstallerTest, BadTypesInManifest_PromptIsNotABoolean) {
  SwReporterInstallerPolicy policy(on_component_ready_callback_);

  // This has an int instead of a bool for prompt.
  static constexpr char kTestManifest[] =
      "{\"launch_params\": ["
      "  {"
      "    \"arguments\": [\"--engine=experimental\"],"
      "    \"suffix\": \"TestSuffix\","
      "    \"prompt\": 1"
      "  }"
      "]}";
  policy.ComponentReady(default_version_, default_path_,
                        base::DictionaryValue::From(
                            base::JSONReader::ReadDeprecated(kTestManifest)));

  // The SwReporter should not be launched, and an error should be logged.
  EXPECT_TRUE(extracted_invocations_.container().empty());
  histograms_.ExpectUniqueSample(kErrorHistogramName,
                                 SW_REPORTER_EXPERIMENT_ERROR_BAD_PARAMS, 1);
}

TEST_F(SwReporterInstallerTest, BadTypesInManifest_LaunchParamsIsScalar) {
  SwReporterInstallerPolicy policy(on_component_ready_callback_);

  static constexpr char kTestManifest[] = "{\"launch_params\": 0}";
  policy.ComponentReady(default_version_, default_path_,
                        base::DictionaryValue::From(
                            base::JSONReader::ReadDeprecated(kTestManifest)));

  // The SwReporter should not be launched, and an error should be logged.
  EXPECT_TRUE(extracted_invocations_.container().empty());
  histograms_.ExpectUniqueSample(kErrorHistogramName,
                                 SW_REPORTER_EXPERIMENT_ERROR_BAD_PARAMS, 1);
}

TEST_F(SwReporterInstallerTest, BadTypesInManifest_LaunchParamsIsDict) {
  SwReporterInstallerPolicy policy(on_component_ready_callback_);

  static constexpr char kTestManifest[] = "{\"launch_params\": {}}";
  policy.ComponentReady(default_version_, default_path_,
                        base::DictionaryValue::From(
                            base::JSONReader::ReadDeprecated(kTestManifest)));

  // The SwReporter should not be launched, and an error should be logged.
  EXPECT_TRUE(extracted_invocations_.container().empty());
  histograms_.ExpectUniqueSample(kErrorHistogramName,
                                 SW_REPORTER_EXPERIMENT_ERROR_BAD_PARAMS, 1);
}

class SwReporterOnDemandFetcherTest : public ::testing::Test,
                                      public OnDemandUpdater {
 public:
  SwReporterOnDemandFetcherTest() = default;

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
    base::ThreadTaskRunnerHandle::Get()->PostTask(
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

  DISALLOW_COPY_AND_ASSIGN(SwReporterOnDemandFetcherTest);
};

TEST_F(SwReporterOnDemandFetcherTest, TestUpdateSuccess) {
  CreateOnDemandFetcherAndVerifyExpectations(true);

  EXPECT_TRUE(on_demand_update_called_);
}

TEST_F(SwReporterOnDemandFetcherTest, TestUpdateFailure) {
  CreateOnDemandFetcherAndVerifyExpectations(false);

  EXPECT_TRUE(on_demand_update_called_);
}

class SwReporterInstallerHistogramTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_CURRENT_USER));

    const base::string16 cleaner_key_name =
        base::StrCat({chrome_cleaner::kSoftwareRemovalToolRegistryKey, L"\\",
                      chrome_cleaner::kCleanerSubKey});
    cleaner_key_ = std::make_unique<base::win::RegKey>(
        HKEY_CURRENT_USER, cleaner_key_name.c_str(),
        KEY_QUERY_VALUE | KEY_SET_VALUE);
    ASSERT_TRUE(cleaner_key_->Valid());
  }

  base::HistogramTester& histograms() { return histograms_; }

  base::win::RegKey& cleaner_key() { return *cleaner_key_; }

  // TODO(crbug.com/872824): use chrome_cleaner::RegistryLogger::WriteStartTime
  //                         once it moves to components/chrome_cleaner
  void WriteStartTime(base::Time start_time) {
    const int64_t serialized =
        start_time.ToDeltaSinceWindowsEpoch().InMicroseconds();
    ASSERT_EQ(ERROR_SUCCESS, cleaner_key().WriteValue(
                                 chrome_cleaner::kStartTimeValueName,
                                 &serialized, sizeof(serialized), REG_QWORD));
  }

  // TODO(crbug.com/872824): use chrome_cleaner::RegistryLogger::WriteEndTime
  //                         once it moves to components/chrome_cleaner
  void WriteEndTime(base::Time end_time) {
    const int64_t serialized =
        end_time.ToDeltaSinceWindowsEpoch().InMicroseconds();
    ASSERT_EQ(ERROR_SUCCESS, cleaner_key().WriteValue(
                                 chrome_cleaner::kEndTimeValueName, &serialized,
                                 sizeof(serialized), REG_QWORD));
  }

 private:
  base::HistogramTester histograms_;
  registry_util::RegistryOverrideManager registry_override_manager_;
  std::unique_ptr<base::win::RegKey> cleaner_key_;
};

TEST_F(SwReporterInstallerHistogramTest, WithStartAndEndTimes) {
  const base::Time start_time =
      base::Time::Now() - base::TimeDelta::FromHours(1);
  const base::Time end_time = start_time + base::TimeDelta::FromSeconds(10);

  WriteStartTime(start_time);
  WriteEndTime(end_time);

  ReportUMAForLastCleanerRun();

  histograms().ExpectUniqueSample("SoftwareReporter.Cleaner.HasCompleted",
                                  1 /* SRT_COMPLETED_YES */, 1);
  histograms().ExpectUniqueSample("SoftwareReporter.Cleaner.RunningTime",
                                  (end_time - start_time).InMilliseconds(), 1);
}

}  // namespace component_updater
