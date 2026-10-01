// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "base/version_info/channel.h"
#include "chrome/browser/ai/ai_data_keyed_service.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/common/features/feature_channel.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace extensions {
namespace {

// Matches the key in api_test/experimental_ai_data/manifest.json.
constexpr char kTestExtensionId[] = "kbanhggbnnaciicfpdkheonkpkeakfal";
enum class Access {
  kAllowed,
  kStableChannel,
  kBlocklisted,
  kNotAllowlisted,
  kIncognitoTarget
};

class ExperimentalAiDataApcApiTest
    : public ExtensionApiTest,
      public testing::WithParamInterface<Access> {
 protected:
  void SetUp() override {
    features_.InitAndEnableFeatureWithParameters(
        AiDataKeyedService::GetAllowlistedAiDataExtensionsFeatureForTesting(),
        {{"allowlisted_extension_ids",
          GetParam() == Access::kNotAllowlisted ? "" : kTestExtensionId},
         {"blocked_extension_ids",
          GetParam() == Access::kBlocklisted ? kTestExtensionId : ""}});
    ExtensionApiTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kExtensionAiDataCollection);
  }

  ScopedCurrentChannel channel_{GetParam() == Access::kStableChannel
                                    ? version_info::Channel::STABLE
                                    : version_info::Channel::DEV};
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_P(ExperimentalAiDataApcApiTest, Snapshot) {
  ASSERT_TRUE(embedded_test_server()->Start());
  // Incognito access must stay disabled even for an incognito-enabled extension
  // making a request from its regular-profile background context.
  auto* target_browser = GetParam() == Access::kIncognitoTarget
                             ? CreateIncognitoBrowser()
                             : browser();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      target_browser, embedded_test_server()->GetURL("/simple.html")));
  auto* contents = target_browser->GetActiveTabInterface()->GetContents();
  ASSERT_TRUE(content::ExecJs(
      contents, "document.body.textContent = 'APC snapshot browser test';"));
  content::WaitForCopyableViewInWebContents(contents);

  base::DictValue config;
  config.Set("tabId", sessions::SessionTabHelper::IdForTab(contents).id());
  switch (GetParam()) {
    case Access::kAllowed:
      break;
    case Access::kStableChannel:
      config.Set("expectedError", "API access not allowed on this channel.");
      break;
    case Access::kBlocklisted:
    case Access::kNotAllowlisted:
      config.Set("expectedError", "API access restricted for this extension.");
      break;
    case Access::kIncognitoTarget:
      config.Set("expectedError", "Invalid target tab passed in.");
      break;
  }
  const std::string custom_arg = base::WriteJson(config).value();
  ASSERT_TRUE(RunExtensionTest(
      "experimental_ai_data", {.custom_arg = custom_arg.c_str()},
      {.allow_in_incognito = GetParam() == Access::kIncognitoTarget}))
      << message_;
}

INSTANTIATE_TEST_SUITE_P(All,
                         ExperimentalAiDataApcApiTest,
                         testing::Values(Access::kAllowed,
                                         Access::kStableChannel,
                                         Access::kBlocklisted,
                                         Access::kNotAllowlisted,
                                         Access::kIncognitoTarget));

}  // namespace
}  // namespace extensions
