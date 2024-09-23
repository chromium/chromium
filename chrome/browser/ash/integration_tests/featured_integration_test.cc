// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>
#include <vector>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/location.h"
#include "base/process/launch.h"
#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/test/base/chromeos/crosier/ash_integration_test.h"
#include "chrome/test/base/chromeos/crosier/chromeos_integration_login_mixin.h"
#include "chrome/test/base/chromeos/crosier/chromeos_test_definition.pb.h"
#include "chrome/test/base/chromeos/crosier/crosier_util.h"
#include "chrome/test/base/chromeos/crosier/helper/test_sudo_helper_client.h"
#include "chrome/test/base/chromeos/crosier/upstart.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

constexpr char kHelperBinary[] =
    "/usr/local/libexec/tast/helpers/local/cros/"
    "featured.FeatureLibraryLateBoot.check";

// The tests start the chromeos_integration_tests binary with two features
// enabled or disabled, with or without parameters, simulating a Finch
// experiment. One feature is default-enabled; the other feature is
// default-disabled. The helper binary (a simulation of a platform binary)
// returns its view of the feature state as a JSON object. The test cases have
// expected output for the default-enabled and default-disabled features.
struct TestCase {
  const char* test_name = "";
  const char* enabled_features = "";
  const char* disabled_features = "";
  const char* expected_default_enabled = "";
  const char* expected_default_disabled = "";
};

class FeaturedIntegrationTest : public AshIntegrationTest,
                                public ::testing::WithParamInterface<TestCase> {
 public:
  FeaturedIntegrationTest() {
    feature_list_.InitFromCommandLine(GetParam().enabled_features,
                                      GetParam().disabled_features);
  }

  // AshIntegrationTest:
  void SetUpOnMainThread() override {
    AshIntegrationTest::SetUpOnMainThread();

    chrome_test_base_chromeos_crosier::TestInfo info;
    info.set_description(
        "Verifies features are enabled/disabled as expected and parameters are "
        "unchanged");
    info.set_team_email("chromeos-data-eng@google.com");
    info.add_contacts("jamescook@google.com");  // Ported from Tast to Crosier.
    info.set_buganizer("1096648");
    crosier_util::AddTestInfo(info);
  }

  base::test::ScopedFeatureList feature_list_;
};

constexpr TestCase kTestCases[] = {
    {.test_name = "experiment_enabled_without_params",
     .enabled_features =
         "CrOSLateBootTestDefaultEnabled,CrOSLateBootTestDefaultDisabled",
     .expected_default_enabled = R"(
        {
          "EnabledCallbackEnabledResult": true,
          "FeatureName": "CrOSLateBootTestDefaultEnabled",
          "ParamsCallbackEnabledResult": true,
          "ParamsCallbackFeatureName": "CrOSLateBootTestDefaultEnabled",
          "ParamsCallbackParamsResult": {}
        }
      )",
     .expected_default_disabled = R"(
        {
          "EnabledCallbackEnabledResult": true,
          "FeatureName": "CrOSLateBootTestDefaultDisabled",
          "ParamsCallbackEnabledResult": true,
          "ParamsCallbackFeatureName": "CrOSLateBootTestDefaultDisabled",
          "ParamsCallbackParamsResult": {}
        }
      )"},
    {.test_name = "experiment_disabled_without_params",
     .disabled_features =
         "CrOSLateBootTestDefaultEnabled,CrOSLateBootTestDefaultDisabled",
     .expected_default_enabled = R"(
        {
          "EnabledCallbackEnabledResult": false,
          "FeatureName": "CrOSLateBootTestDefaultEnabled",
          "ParamsCallbackEnabledResult": false,
          "ParamsCallbackFeatureName": "CrOSLateBootTestDefaultEnabled",
          "ParamsCallbackParamsResult": {}
        }
      )",
     .expected_default_disabled = R"(
        {
          "EnabledCallbackEnabledResult": false,
          "FeatureName": "CrOSLateBootTestDefaultDisabled",
          "ParamsCallbackEnabledResult": false,
          "ParamsCallbackFeatureName": "CrOSLateBootTestDefaultDisabled",
          "ParamsCallbackParamsResult": {}
        }
      )"},
    {.test_name = "experiment_enabled_with_params",
     .enabled_features = "CrOSLateBootTestDefaultEnabled:k1/v1/k2/v2,"
                         "CrOSLateBootTestDefaultDisabled:k3/v3/k4/v4",
     .expected_default_enabled = R"(
        {
          "EnabledCallbackEnabledResult": true,
          "FeatureName": "CrOSLateBootTestDefaultEnabled",
          "ParamsCallbackEnabledResult": true,
          "ParamsCallbackFeatureName": "CrOSLateBootTestDefaultEnabled",
          "ParamsCallbackParamsResult": {
            "k1":"v1",
            "k2":"v2"
          }
        }
      )",
     .expected_default_disabled = R"(
        {
          "EnabledCallbackEnabledResult": true,
          "FeatureName": "CrOSLateBootTestDefaultDisabled",
          "ParamsCallbackEnabledResult": true,
          "ParamsCallbackFeatureName": "CrOSLateBootTestDefaultDisabled",
          "ParamsCallbackParamsResult": {
            "k3":"v3",
            "k4":"v4"
          }
        }
      )"},
    {.test_name = "experiment_default",
     .enabled_features = "",
     .expected_default_enabled = R"(
        {
          "EnabledCallbackEnabledResult": true,
          "FeatureName": "CrOSLateBootTestDefaultEnabled",
          "ParamsCallbackEnabledResult": true,
          "ParamsCallbackFeatureName": "CrOSLateBootTestDefaultEnabled",
          "ParamsCallbackParamsResult": {}
        }
      )",
     .expected_default_disabled = R"(
        {
          "EnabledCallbackEnabledResult": false,
          "FeatureName": "CrOSLateBootTestDefaultDisabled",
          "ParamsCallbackEnabledResult": false,
          "ParamsCallbackFeatureName": "CrOSLateBootTestDefaultDisabled",
          "ParamsCallbackParamsResult": {}
        }
      )"},

};

const char* NameFromTestCase(::testing::TestParamInfo<TestCase> info) {
  return info.param.test_name;
}

INSTANTIATE_TEST_SUITE_P(All,
                         FeaturedIntegrationTest,
                         ::testing::ValuesIn(kTestCases),
                         NameFromTestCase);

IN_PROC_BROWSER_TEST_P(FeaturedIntegrationTest, FeatureLibraryLateBoot) {
  // Collect stdout from the helper binary. The binary will call back via D-Bus
  // into chromeos_integration_tests, so collect the binary's output off the
  // main thread.
  std::string output;
  base::RunLoop run_loop;
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(
          [](std::string* output, base::RunLoop* run_loop) {
            // Run the helper binary to get its feature state.
            ASSERT_TRUE(base::GetAppOutput({kHelperBinary}, output));
            run_loop->Quit();
          },
          &output, &run_loop));
  run_loop.Run();

  // The helper binary returns two lines of output.
  std::vector<std::string> split_out = base::SplitString(
      output, "\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  ASSERT_EQ(split_out.size(), 2u);

  // The first line of the output is the state of the default enabled feature.
  std::optional<base::Value> default_enabled =
      base::JSONReader::Read(split_out[0]);
  ASSERT_TRUE(default_enabled.has_value());

  // The test case has the expected JSON for the default enabled feature.
  std::optional<base::Value> expected_default_enabled =
      base::JSONReader::Read(GetParam().expected_default_enabled);
  ASSERT_TRUE(expected_default_enabled.has_value());
  EXPECT_EQ(default_enabled, expected_default_enabled);

  // The second line of the output is the default disabled feature.
  std::optional<base::Value> default_disabled =
      base::JSONReader::Read(split_out[1]);
  ASSERT_TRUE(default_disabled.has_value());

  // The test case has the expected JSON for the default disabled feature.
  std::optional<base::Value> expected_default_disabled =
      base::JSONReader::Read(GetParam().expected_default_disabled);
  ASSERT_TRUE(expected_default_disabled.has_value());
  EXPECT_EQ(default_disabled, expected_default_disabled);
}

////////////////////////////////////////////////////////////////////////////////
// FeaturedLatePlatformIntegrationTest
////////////////////////////////////////////////////////////////////////////////

// kTestFeature is the name of the test feature in platform-features.json.
// It should always match the name in that file exactly.
constexpr char kTestFeature[] = "CrOSLateBootTestFeature";

// kDirPath is the path to the directory this test uses.
constexpr char kDirPath[] = "/run/featured_test";

// kFilePath is the file whose existence gates the behavior of featured.
// If it exists and the experiment is enabled, featured should write a string to
// it. Otherwise, it should do nothing.
constexpr char kFilePath[] = "/run/featured_test/test_write";

// kExpectedContents is the expected contents of the filePath after featured
// writes to it.
constexpr char kExpectedContents[] = "test_featured";

class FeaturedLatePlatformIntegrationTest
    : public AshIntegrationTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  // Whether the test file should be created by the test.
  static bool TestFileExists() { return std::get<0>(GetParam()); }

  // Whether the experimental feature should be enabled in chrome.
  static bool TestFeatureEnabled() { return std::get<1>(GetParam()); }

  FeaturedLatePlatformIntegrationTest() {
    // Don't enable or disable the test feature in the PRE_ test stage. Only do
    // it in the main test stage. Ditto for login.
    if (base::StartsWith(
            testing::UnitTest::GetInstance()->current_test_info()->name(),
            "PRE_")) {
      return;
    }
    if (TestFeatureEnabled()) {
      feature_list_.InitFromCommandLine(/*enable_features=*/kTestFeature,
                                        /*disable_features=*/"");
    } else {
      feature_list_.InitFromCommandLine(/*enable_features=*/"",
                                        /*disable_features=*/kTestFeature);
    }
    set_exit_when_last_browser_closes(false);
    login_mixin().SetMode(ChromeOSIntegrationLoginMixin::Mode::kTestLogin);
  }

  void TearDownOnMainThread() override {
    // Don't clean up files in the PRE_ test stage because they must continue to
    // exist for the main test stage.
    if (base::StartsWith(
            testing::UnitTest::GetInstance()->current_test_info()->name(),
            "PRE_")) {
      return;
    }

    // The directory is owned by root, so use the sudo helper.
    auto result = TestSudoHelperClient().RunCommand(
        base::StringPrintf("rm -rf %s", kDirPath));
    ASSERT_EQ(result.return_code, 0) << result.output;

    AshIntegrationTest::TearDownOnMainThread();
  }

  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(FileFeature,
                         FeaturedLatePlatformIntegrationTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

IN_PROC_BROWSER_TEST_P(FeaturedLatePlatformIntegrationTest,
                       PRE_LatePlatformFeatures) {
  // Use sudo helper because /run is owned by root.
  TestSudoHelperClient sudo;
  auto result = sudo.RunCommand(base::StringPrintf("mkdir -p %s", kDirPath));
  ASSERT_EQ(result.return_code, 0) << result.output;
  result = sudo.RunCommand(base::StringPrintf("chmod 755 %s", kDirPath));
  ASSERT_EQ(result.return_code, 0) << result.output;

  base::FilePath file_path(kFilePath);
  if (TestFileExists()) {
    // Create a zero-sized file.
    result = sudo.RunCommand(base::StringPrintf("truncate -s 0 %s", kFilePath));
    ASSERT_EQ(result.return_code, 0) << result.output;

    result = sudo.RunCommand(base::StringPrintf("chmod 664 %s", kFilePath));
    ASSERT_EQ(result.return_code, 0) << result.output;
  } else {
    // Ensure the file does not exist.
    result = sudo.RunCommand("rm -f /run/featured_test/test_write");
    ASSERT_EQ(result.return_code, 0) << result.output;
  }

  // Restart featured.
  ASSERT_TRUE(upstart::RestartJob("featured"));
  ASSERT_TRUE(upstart::WaitForJobStatus("featured", upstart::Goal::kStart,
                                        upstart::State::kRunning,
                                        upstart::WrongGoalPolicy::kReject));

  // Restart chrome (by ending this PRE_ test).
}

IN_PROC_BROWSER_TEST_P(FeaturedLatePlatformIntegrationTest,
                       LatePlatformFeatures) {
  login_mixin().Login();
  ash::test::WaitForPrimaryUserSessionStart();
  EXPECT_TRUE(login_mixin().IsCryptohomeMounted());

  base::ScopedAllowBlockingForTesting allow_blocking;

  // Poll for files for up to 20 seconds.
  bool passed = false;
  for (int i = 0; i < 20; ++i) {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), base::Seconds(1));
    run_loop.Run();

    base::FilePath file_path(kFilePath);
    if (TestFileExists()) {
      // If testing the file exists case, the file should exist. Featured should
      // not have removed the file.
      ASSERT_TRUE(base::PathExists(file_path));
      std::string contents;
      ASSERT_TRUE(base::ReadFileToString(file_path, &contents));
      if (TestFeatureEnabled()) {
        // If the file contains the expected string, exit the loop. If not, keep
        // polling.
        if (contents == kExpectedContents) {
          passed = true;
          break;
        }
      } else {
        // In the feature disabled case the File should be empty. If it is,
        // exit the loop. Otherwise keep polling.
        if (contents.empty()) {
          passed = true;
          break;
        }
      }
    } else {
      // If testing the file does-not-exist case, featured should not have
      // created the file.
      ASSERT_FALSE(base::PathExists(file_path));
      passed = true;
      // Continue to poll, to ensure featured doesn't create the file later.
    }
  }
  EXPECT_TRUE(passed) << "Did not pass after 20 iterations.";
}

}  // namespace
}  // namespace ash
