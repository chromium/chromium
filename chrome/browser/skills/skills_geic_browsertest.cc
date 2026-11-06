// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/strings/escape.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_provider.h"
#include "chrome/browser/glic/public/glic_provider_latch.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/skills/skills_service_factory.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/skills/features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace skills {
namespace {

constexpr char kGeicPolicyUrl[] =
    "https://business.gemini.google/side-panel?"
    "configId=abc123&location=global&mods=widget_staging_api_mod&jsmode=du";
constexpr char kExpectedExternalSkillsUrl[] =
    "https://business.gemini.google/home/cid/abc123/r/skills?"
    "mods=widget_staging_api_mod&jsmode=du";

std::vector<base::test::FeatureRef> GetEnabledFeatures() {
  std::vector<base::test::FeatureRef> features =
      glic::GetDefaultEnabledGlicTestFeatures();
  features.push_back(::features::kSkillsEnabled);
  features.push_back(::features::kSkillsAndExtensionsAppMenu);
  features.push_back(::features::kGeic);
  return features;
}

// Sets the GeminiEnterpriseUrl policy before the browser starts, so the
// profile's Glic provider is latched as Gemini Enterprise.
class SkillsGeicBrowserTest : public InProcessBrowserTest {
 public:
  SkillsGeicBrowserTest()
      : glic_test_env_(glic::GlicTestEnvironmentConfig{},
                       GetEnabledFeatures()) {}

  void SetUpInProcessBrowserTestFixture() override {
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::PolicyMap policies;
    policies.Set(policy::key::kGeminiEnterpriseUrl,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, base::Value(kGeicPolicyUrl),
                 nullptr);
    policy_provider_.UpdateChromePolicy(policies);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
  }

 protected:
  content::WebContents* web_contents() {
    return browser()->GetTabStripModel()->GetActiveWebContents();
  }

 private:
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
  glic::GlicTestEnvironment glic_test_env_;
};

// Skills are disabled for Gemini Enterprise, even though Glic is enabled.
IN_PROC_BROWSER_TEST_F(SkillsGeicBrowserTest, SkillsDisabled) {
  Profile* profile = browser()->GetProfile();
  ASSERT_EQ(glic::GlicProviderLatch::GetProvider(*profile),
            glic::GlicProvider::kGeminiEnterprise);
  ASSERT_TRUE(glic::GlicEnabling::IsEnabledForProfile(profile));

  EXPECT_FALSE(SkillsServiceFactory::IsSkillsEnabledForProfile(profile));
  EXPECT_EQ(SkillsServiceFactory::GetForProfile(profile), nullptr);

  actions::ActionItem* skills_and_extensions_action =
      actions::ActionManager::Get().FindAction(
          kActionSkillsAndExtensionsSubmenu,
          BrowserActions::From(browser())->root_action_item());
  ASSERT_NE(skills_and_extensions_action, nullptr);
  EXPECT_FALSE(skills_and_extensions_action->GetVisible());

  AppMenuModel model(/*provider=*/nullptr, browser());
  model.Init();
  EXPECT_FALSE(model
                   .GetIndexOfCommandId(
                       AppMenuModel::kSkillsAndExtensionsMenuPlaceholder)
                   .has_value());
}

// chrome://skills shows the unsupported error page with a link to Gemini
// Enterprise skills for Gemini Enterprise profiles.
IN_PROC_BROWSER_TEST_F(SkillsGeicBrowserTest, SkillsPageShowsUnsupportedError) {
  ASSERT_EQ(glic::GlicProviderLatch::GetProvider(*browser()->GetProfile()),
            glic::GlicProvider::kGeminiEnterprise);

  ASSERT_TRUE(
      content::NavigateToURL(web_contents(), GURL(chrome::kChromeUISkillsURL)));

  constexpr char kErrorPageJs[] =
      "(document.querySelector('error-page') ?? "
      "document.querySelector('skills-app').shadowRoot"
      ".querySelector('error-page'))";
  EXPECT_EQ("skills-unsupported",
            content::EvalJs(web_contents(),
                            std::string(kErrorPageJs) + ".errorType"));
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_SKILLS_UNSUPPORTED_ERROR_PAGE_TITLE),
            content::EvalJs(web_contents(),
                            std::string(kErrorPageJs) +
                                ".shadowRoot.querySelector('#errorTitle')"
                                ".textContent"));
  EXPECT_EQ(
      l10n_util::GetStringFUTF8(
          IDS_SKILLS_UNSUPPORTED_ERROR_PAGE_DESCRIPTION,
          base::UTF8ToUTF16(base::EscapeForHTML(kExpectedExternalSkillsUrl))),
      content::EvalJs(web_contents(),
                      std::string(kErrorPageJs) +
                          ".shadowRoot.querySelector('.body-text')"
                          ".innerHTML"));
  EXPECT_EQ(kExpectedExternalSkillsUrl,
            content::EvalJs(web_contents(),
                            std::string(kErrorPageJs) +
                                ".shadowRoot.querySelector('.body-text a')"
                                ".href"));
  EXPECT_EQ("Gemini Enterprise web app",
            content::EvalJs(web_contents(),
                            std::string(kErrorPageJs) +
                                ".shadowRoot.querySelector('.body-text a')"
                                ".textContent"));
}

}  // namespace
}  // namespace skills
