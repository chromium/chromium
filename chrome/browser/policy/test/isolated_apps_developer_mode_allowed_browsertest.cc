// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/base_switches.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using PolicyVerdictPair = std::tuple<policy::PolicyTest::BooleanPolicy, bool>;
using BooleanPolicy = policy::PolicyTest::BooleanPolicy;

base::FilePath::StringType kSimpleIsolatedWebAppPath =
    FILE_PATH_LITERAL("web_apps/simple_isolated_app");

class IsolatedWebAppsDeveloperModeAllowedPolicyTest
    : public web_app::IsolatedWebAppBrowserTestHarness,
      public testing::WithParamInterface<PolicyVerdictPair> {
 public:
  IsolatedWebAppsDeveloperModeAllowedPolicyTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kIsolatedWebApps);
  }

  void SetUpInProcessBrowserTestFixture() override {
    provider_.SetDefaultReturns(
        true /* is_initialization_complete_return */,
        true /* is_first_policy_load_complete_return */);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
    provider_.UpdateChromePolicy(
        GenerateIsolatedWebAppsDeveloperModeAllowedPolicy());
  }

 private:
  policy::PolicyMap GenerateIsolatedWebAppsDeveloperModeAllowedPolicy() {
    policy::PolicyMap policies;

    const BooleanPolicy policy = std::get<0>(GetParam());
    if (policy != BooleanPolicy::kNotConfigured) {
      const bool policy_bool = (policy == BooleanPolicy::kTrue);
      policies.Set(policy::key::kIsolatedAppsDeveloperModeAllowed,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_ENTERPRISE_DEFAULT,
                   base::Value(policy_bool), nullptr);
    }

    return policies;
  }

  testing::NiceMock<policy::MockConfigurationPolicyProvider> provider_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(IsolatedWebAppsDeveloperModeAllowedPolicyTest, MockTcp) {
  std::unique_ptr<net::EmbeddedTestServer> isolated_web_app_dev_server =
      CreateAndStartServer(kSimpleIsolatedWebAppPath);
  web_app::IsolatedWebAppUrlInfo url_info = InstallDevModeProxyIsolatedWebApp(
      isolated_web_app_dev_server->GetOrigin());
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  content::RenderFrameHost* navigated_frame = ui_test_utils::NavigateToURL(
      GetBrowserFromFrame(app_frame),
      url_info.origin().GetURL().Resolve("/direct_sockets.html"));

  const bool enabled = std::get<1>(GetParam());
  ASSERT_EQ(enabled, EvalJs(navigated_frame, "mockTcp()"));
}

IN_PROC_BROWSER_TEST_P(IsolatedWebAppsDeveloperModeAllowedPolicyTest, MockUdp) {
  std::unique_ptr<net::EmbeddedTestServer> isolated_web_app_dev_server =
      CreateAndStartServer(kSimpleIsolatedWebAppPath);
  web_app::IsolatedWebAppUrlInfo url_info = InstallDevModeProxyIsolatedWebApp(
      isolated_web_app_dev_server->GetOrigin());
  content::RenderFrameHost* app_frame = OpenApp(url_info.app_id());

  content::RenderFrameHost* navigated_frame = ui_test_utils::NavigateToURL(
      GetBrowserFromFrame(app_frame),
      url_info.origin().GetURL().Resolve("/direct_sockets.html"));

  const bool enabled = std::get<1>(GetParam());
  ASSERT_EQ(enabled, EvalJs(navigated_frame, "mockUdp()"));
}

INSTANTIATE_TEST_SUITE_P(
    /*empty*/,
    IsolatedWebAppsDeveloperModeAllowedPolicyTest,
    ::testing::Values(
        PolicyVerdictPair{policy::PolicyTest::BooleanPolicy::kNotConfigured,
                          true},
        PolicyVerdictPair{policy::PolicyTest::BooleanPolicy::kFalse, false},
        PolicyVerdictPair{policy::PolicyTest::BooleanPolicy::kTrue, true}));
