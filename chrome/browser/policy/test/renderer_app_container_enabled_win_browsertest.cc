// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/numerics/safe_conversions.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "sandbox/features.h"
#include "sandbox/policy/features.h"
#include "sandbox/policy/switches.h"
#include "sandbox/policy/win/sandbox_win.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace policy {

class RendererAppContainerEnabledTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<
          /*policy::key::kRendererAppContainerEnabled=*/std::optional<bool>> {
 public:
  // InProcessBrowserTest implementation:
  void SetUp() override {
    // Only certain Windows versions support App Container.
    if (!sandbox::features::IsAppContainerSandboxSupported())
      GTEST_SKIP();

    // Need sandbox to be enabled.
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            sandbox::policy::switches::kNoSandbox)) {
      GTEST_SKIP();
    }

    enable_app_container_.InitAndEnableFeature(
        sandbox::policy::features::kRendererAppContainer);
    ASSERT_TRUE(embedded_test_server()->Start());

    policy_provider_.SetDefaultReturns(
        true /* is_initialization_complete_return */,
        true /* is_first_policy_load_complete_return */);
    policy::PolicyMap values;
    if (GetParam().has_value()) {
      values.Set(policy::key::kRendererAppContainerEnabled,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
                 policy::POLICY_SOURCE_CLOUD, base::Value(*GetParam()),
                 nullptr);
    }
    policy_provider_.UpdateChromePolicy(values);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
    InProcessBrowserTest::SetUp();
  }

 protected:
  bool GetExpectedResult() {
    // If policy is not set, then App Container is enabled by default.
    if (!GetParam().has_value())
      return true;
    // Otherwise, return the value of the policy.
    return *GetParam();
  }

 private:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
  base::test::ScopedFeatureList enable_app_container_;
};

IN_PROC_BROWSER_TEST_P(RendererAppContainerEnabledTest, IsRespected) {
  GURL url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Duplicate the base::Process to keep a valid Windows handle to to the
  // process open, this ensures that even if the RPH gets destroyed during the
  // runloop below, the handle to the process remains valid, and the pid is
  // never reused by Windows.
  const auto renderer_process = browser()
                                    ->tab_strip_model()
                                    ->GetActiveWebContents()
                                    ->GetPrimaryMainFrame()
                                    ->GetProcess()
                                    ->GetProcess()
                                    .Duplicate();

  base::RunLoop run_loop;
  base::Value out_args;
  sandbox::policy::SandboxWin::GetPolicyDiagnostics(
      base::BindLambdaForTesting([&run_loop, &out_args](base::Value args) {
        out_args = std::move(args);
        run_loop.Quit();
      }));
  run_loop.Run();

  const base::Value::List* process_list = out_args.GetIfList();
  ASSERT_TRUE(process_list);
  bool found_renderer = false;
  bool found_lowbox_renderer = false;
  for (const base::Value& process_value : *process_list) {
    const base::Value::Dict* process = process_value.GetIfDict();
    ASSERT_TRUE(process);
    std::optional<double> pid = process->FindDouble("processId");
    ASSERT_TRUE(pid.has_value());
    if (base::checked_cast<base::ProcessId>(pid.value()) !=
        renderer_process.Pid()) {
      continue;
    }
    found_renderer = true;
    auto* lowbox_sid = process->FindString("lowboxSid");
    if (!lowbox_sid)
      continue;
    // App container SIDs will start with S-1-15-2.
    if (base::StartsWith(*lowbox_sid, "S-1-15-2")) {
      found_lowbox_renderer = true;
      break;
    }
  }

  EXPECT_TRUE(found_renderer);
  EXPECT_EQ(GetExpectedResult(), found_lowbox_renderer);
}

INSTANTIATE_TEST_SUITE_P(
    Enabled,
    RendererAppContainerEnabledTest,
    ::testing::Values(/*policy::key::kRendererAppContainerEnabled=*/true));

INSTANTIATE_TEST_SUITE_P(
    Disabled,
    RendererAppContainerEnabledTest,
    ::testing::Values(/*policy::key::kRendererAppContainerEnabled=*/false));

INSTANTIATE_TEST_SUITE_P(
    NotSet,
    RendererAppContainerEnabledTest,
    ::testing::Values(
        /*policy::key::kRendererAppContainerEnabled=*/std::nullopt));

}  // namespace policy
