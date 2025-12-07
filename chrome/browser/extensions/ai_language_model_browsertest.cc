// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>
#include <vector>

#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/to_string.h"
#include "base/test/scoped_feature_list.h"
#include "base/version_info/channel.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"

namespace extensions {

namespace {

// The `key` field stores the public key for the extension with id
// "jnapclmfkaejhjkddbmiafekigmcbmma".
static constexpr char kManifestTemplate[] =
    R"JS(
    {
      "name": "AI language model test",
      "version": "0.1",
      "manifest_version": 3,
      "key": "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA3H6Jc0On6l0H3DJ6bx4aOW3+srCfjSdr+3ukwIEZrL6jDy500XweIwOp9PItpM9sijwu8v1rdyoBPubm/ottp/oz42aKp+2xIxcMTa6/cA2BL2kOWxwv+WP9d01IOFbFpWmQBDQNpp2UmH67OFbie6zHhyrSJKL2o9d05iX0a9Xwv9W48JKYpldo+/2JTP/5en0jxgiN+qkOCZuLag2cS/6Az0LArqsf5D+ReJemIBCNJhVxu3P0naxfEG6B6XczzuuptrX3H2vDr1LxZasLh9bzV88+8BxarjETACebOfqy366QxXluwAjnu/NHPv53edXlXvXrZ0C69RvvlMh1qQIDAQAB",
      "description": "Extension for testing the AI language model API.",
      "background": {
        "service_worker": "sw.js"
      }
    }
  )JS";

// The boolean tuple describing:
// 1. if the `kAIPromptAPI` chrome://flag is explicitly enabled;
// 2. if the `kAIPromptAPI` kill switch is triggered;
// 3. if the `kAIPromptAPIForExtension` kill switch is triggered;
using Variant = std::tuple<bool, bool, bool>;
bool IsAPIFlagEnabled(Variant v) {
  return std::get<0>(v);
}
bool IsAPIKillSwitchTriggered(Variant v) {
  return std::get<1>(v);
}
bool IsExtensionKillSwitchTriggered(Variant v) {
  return std::get<2>(v);
}

// Describes the test variants in a meaningful way in the parameterized tests.
std::string DescribeTestVariant(const testing::TestParamInfo<Variant> info) {
  std::string api_flag_enabled =
      IsAPIFlagEnabled(info.param) ? "WithAPIFlag" : "NoAPIFlag";
  std::string api_kill_switch = IsAPIKillSwitchTriggered(info.param)
                                    ? "WithAPIKillswitch"
                                    : "NoAPIKillswitch";
  std::string extension_kill_switch = IsExtensionKillSwitchTriggered(info.param)
                                          ? "WithExtensionKillswitch"
                                          : "NoExtensionKillswitch";
  return base::JoinString(
      {api_flag_enabled, api_kill_switch, extension_kill_switch}, "_");
}

}  // namespace

// TODO(crbug.com/419321441): Support Built-In AI APIs on ChromeOS.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_ExtensionAILanguageModelBrowserTest \
  DISABLED_ExtensionAILanguageModelBrowserTest
#else
#define MAYBE_ExtensionAILanguageModelBrowserTest \
  ExtensionAILanguageModelBrowserTest
#endif  // BUILDFLAG(IS_CHROMEOS)
class MAYBE_ExtensionAILanguageModelBrowserTest
    : public ExtensionBrowserTest,
      public testing::WithParamInterface<Variant> {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionBrowserTest::SetUpCommandLine(command_line);
    if (IsAPIFlagEnabled(GetParam())) {
      command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                      "AIPromptAPI");
    }

    base::flat_map<base::test::FeatureRef, bool> feature_states;
    if (IsAPIKillSwitchTriggered(GetParam())) {
      feature_states[blink::features::kAIPromptAPI] = false;
    }
    if (IsExtensionKillSwitchTriggered(GetParam())) {
      feature_states[blink::features::kAIPromptAPIForExtension] = false;
    }
    feature_list_.InitWithFeatureStates(feature_states);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    MAYBE_ExtensionAILanguageModelBrowserTest,
    testing::Combine(testing::Bool(), testing::Bool(), testing::Bool()),
    &DescribeTestVariant);

// Check whether the API is exposed to the extension worker when expected.
IN_PROC_BROWSER_TEST_P(MAYBE_ExtensionAILanguageModelBrowserTest,
                       ExposedToWorker) {
  static constexpr char kScript[] = R"JS(
    chrome.test.runTests([
      function verifyLanguageModelExposed() {
        const expectLanguageModel = %s;
        chrome.test.assertEq(expectLanguageModel, !!self.LanguageModel);
        chrome.test.succeed();
      },
    ]);
  )JS";
  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifestTemplate);
  // Extension access is blocked by either kill switch.
  const bool is_api_exposed = IsAPIFlagEnabled(GetParam()) ||
                              (!IsAPIKillSwitchTriggered(GetParam()) &&
                               !IsExtensionKillSwitchTriggered(GetParam()));
  test_dir.WriteFile(
      FILE_PATH_LITERAL("sw.js"),
      base::StringPrintf(kScript, base::ToString(is_api_exposed)));
  ResultCatcher result_catcher;
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

// Invoke availability() for basic API functionality coverage beyond WPTs.
IN_PROC_BROWSER_TEST_P(MAYBE_ExtensionAILanguageModelBrowserTest,
                       AvailableInWorker) {
  static constexpr char kScript[] = R"JS(
    chrome.test.runTests([
      async function verifyLanguageModelAvailability() {
        if (!!self.LanguageModel) {  // Skip checking when not exposed.
          const availability = await LanguageModel.availability();
          chrome.test.assertEq(typeof(availability), 'string');
        }
        chrome.test.succeed();
      },
    ]);
  )JS";
  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifestTemplate);
  test_dir.WriteFile(FILE_PATH_LITERAL("sw.js"), kScript);
  ResultCatcher result_catcher;
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
  // TODO(crbug.com/421031829): Resolve underlying issue behind UnloadExtension.
  UnloadExtension(extension->id());
}

}  // namespace extensions
