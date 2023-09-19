// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/process/launch.h"
#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "base/task/task_traits.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/test/base/chromeos/crosier/chromeos_test_definition.pb.h"
#include "chrome/test/base/chromeos/crosier/crosier_util.h"
#include "chrome/test/base/chromeos/crosier/interactive_ash_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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

class FeaturedIntegrationTest : public InteractiveAshTest,
                                public ::testing::WithParamInterface<TestCase> {
 public:
  FeaturedIntegrationTest() {
    feature_list_.InitFromCommandLine(GetParam().enabled_features,
                                      GetParam().disabled_features);
  }

  // InteractiveAshTest:
  void SetUpOnMainThread() override {
    InteractiveAshTest::SetUpOnMainThread();

    chrome_test_base_chromeos_crosier::TestInfo info;
    info.set_description(
        "Verifies features are enabled/disabled as expected and parameters are "
        "unchanged");
    info.set_team_email("cros-telemetry@google.com");
    info.add_contacts("kendraketsui@google.com");
    info.add_contacts("mutexlox@google.com");
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
  absl::optional<base::Value> default_enabled =
      base::JSONReader::Read(split_out[0]);
  ASSERT_TRUE(default_enabled.has_value());

  // The test case has the expected JSON for the default enabled feature.
  absl::optional<base::Value> expected_default_enabled =
      base::JSONReader::Read(GetParam().expected_default_enabled);
  ASSERT_TRUE(expected_default_enabled.has_value());
  EXPECT_EQ(default_enabled, expected_default_enabled);

  // The second line of the output is the default disabled feature.
  absl::optional<base::Value> default_disabled =
      base::JSONReader::Read(split_out[1]);
  ASSERT_TRUE(default_disabled.has_value());

  // The test case has the expected JSON for the default disabled feature.
  absl::optional<base::Value> expected_default_disabled =
      base::JSONReader::Read(GetParam().expected_default_disabled);
  ASSERT_TRUE(expected_default_disabled.has_value());
  EXPECT_EQ(default_disabled, expected_default_disabled);
}

}  // namespace
}  // namespace ash
