// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <string>

#include "base/json/json_reader.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/network_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace policy {
namespace {

using ::testing::UnorderedPointwise;

const char* kHostA = "a.test";
const char* kHostB = "b.test";
const char* kHostC = "c.test";
const char* kHostD = "d.test";

class EnabledPolicyBrowsertest
    : public PolicyTest,
      public ::testing::WithParamInterface<std::tuple<
          PolicyTest::BooleanPolicy,  // FirstPartySetsEnabled Policy State
          PolicyTest::BooleanPolicy,  // RelatedWebsiteSetsEnabled Policy State
          const char*                 // Overrides Policy
          >> {
 public:
  EnabledPolicyBrowsertest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    scoped_feature_list_.InitWithFeatures(
        {}, {content_settings::features::kTrackingProtection3pcd});
  }

  void SetBlockThirdPartyCookies(bool value) {
    browser()->profile()->GetPrefs()->SetInteger(
        prefs::kCookieControlsMode,
        static_cast<int>(
            value ? content_settings::CookieControlsMode::kBlockThirdParty
                  : content_settings::CookieControlsMode::kOff));
  }

  void SetUpOnMainThread() override {
    PolicyTest::SetUpOnMainThread();
    // Add content/test/data for cross_site_iframe_factory.html
    https_server_.ServeFilesFromSourceDirectory("content/test/data");
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_.Start());

    permissions::PermissionRequestManager* manager =
        permissions::PermissionRequestManager::FromWebContents(
            browser()->tab_strip_model()->GetActiveWebContents());
    prompt_factory_ =
        std::make_unique<permissions::MockPermissionPromptFactory>(manager);
    prompt_factory_->set_response_type(
        permissions::PermissionRequestManager::DISMISS);

    SetBlockThirdPartyCookies(true);
  }

  void TearDownOnMainThread() override { prompt_factory_.reset(); }

  void SetUpInProcessBrowserTestFixture() override {
    PolicyTest::SetUpInProcessBrowserTestFixture();
    if (std::optional<std::string> policy = GetOverridesPolicy();
        policy.has_value()) {
      SetPolicyValue(GetOverridesPolicyName(),
                     base::JSONReader::Read(policy.value()));
    }

    if (GetInitialFirstPartySetPolicyState() !=
        PolicyTest::BooleanPolicy::kNotConfigured) {
      SetPolicyValue(policy::key::kFirstPartySetsEnabled,
                     base::Value(GetInitialFirstPartySetPolicyState() ==
                                 PolicyTest::BooleanPolicy::kTrue));
    }
    if (GetInitialRelatedWebsiteSetPolicyState() !=
        PolicyTest::BooleanPolicy::kNotConfigured) {
      SetPolicyValue(policy::key::kRelatedWebsiteSetsEnabled,
                     base::Value(GetInitialRelatedWebsiteSetPolicyState() ==
                                 PolicyTest::BooleanPolicy::kTrue));
    }

    provider_.UpdateChromePolicy(policies_);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    PolicyTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        network::switches::kUseRelatedWebsiteSet,
        base::StringPrintf(R"({"primary": "https://%s",)"
                           R"("associatedSites": ["https://%s","https://%s"]})",
                           kHostA, kHostB, kHostC));
  }

  content::RenderFrameHost* GetPrimaryMainFrame() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    return web_contents->GetPrimaryMainFrame();
  }

  content::RenderFrameHost* GetFrame() {
    return ChildFrameAt(GetPrimaryMainFrame(), 0);
  }

 protected:
  virtual std::optional<std::string> GetOverridesPolicy() {
    return std::nullopt;
  }

  // Sets the state of the RelatedWebsiteSetsEnabled Policy to
  // `relatedWebsiteSetsEnabled`. Once the RelatedWebsiteSetsEnabled Policy's
  // state is set, the FirstPartySetsEnabled Policy's state is not used anymore,
  // so we're intentionally setting it's state to `!relatedWebsiteSetsEnabled`,
  // and tests should still pass.
  void SetEnabledPolicyStates(bool relatedWebsiteSetsEnabled) {
    SetPolicyValue(policy::key::kRelatedWebsiteSetsEnabled,
                   base::Value(relatedWebsiteSetsEnabled));
    SetPolicyValue(policy::key::kFirstPartySetsEnabled,
                   base::Value(!relatedWebsiteSetsEnabled));

    provider_.UpdateChromePolicy(policies_);
  }

  // Returns whether or not Related Website Sets (fka First-Party Sets) was
  // enabled at the start of the test. This does not account for calls to
  // `SetEnabledPolicyStates`.
  bool IsRelatedWebsiteSetsEnabledInitially() {
    return IsPrefEnabledInitially();
  }

  void NavigateToPageWithFrame(const std::string& host) {
    GURL main_url(https_server_.GetURL(host, "/iframe.html"));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
  }

  void NavigateFrameTo(const GURL& url) {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_TRUE(NavigateIframeToURL(web_contents, "test", url));
  }

  bool AreSitesInSameRelatedWebsiteSet(const std::string& first_host,
                                       const std::string& second_host) {
    NavigateToPageWithFrame(first_host);
    NavigateFrameTo(https_server_.GetURL(second_host, "/empty.html"));

    return content::ExecJs(GetFrame(), "document.requestStorageAccess()");
  }

  PolicyTest::BooleanPolicy GetInitialFirstPartySetPolicyState() {
    return std::get<0>(GetParam());
  }
  PolicyTest::BooleanPolicy GetInitialRelatedWebsiteSetPolicyState() {
    return std::get<1>(GetParam());
  }
  const char* GetOverridesPolicyName() { return std::get<2>(GetParam()); }

  // If the RelatedWebsiteSetEnabled policy is unset
  // SimpleDeprecatingPolicyHandler falls back to the FirstPartySetEnabled
  // policy so we infer the Pref's state accordingly.
  bool IsPrefEnabledInitially() {
    if (GetInitialRelatedWebsiteSetPolicyState() ==
        PolicyTest::BooleanPolicy::kNotConfigured) {
      return GetInitialFirstPartySetPolicyState() !=
             PolicyTest::BooleanPolicy::kFalse;
    }

    return GetInitialRelatedWebsiteSetPolicyState() !=
           PolicyTest::BooleanPolicy::kFalse;
  }

 private:
  void SetPolicyValue(const char* key, std::optional<base::Value> value) {
    // POLICY_LEVEL_MANDATORY - since administrators will control FPS policy
    // POLICY_SCOPE_USER - since this policy is per profile, not on local state
    // POLICY_SOURCE_ENTERPRISE_DEFAULT - since this is an enterprise policy
    policies_.Set(key, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                  POLICY_SOURCE_ENTERPRISE_DEFAULT, std::move(value), nullptr);
  }

  std::unique_ptr<permissions::MockPermissionPromptFactory> prompt_factory_;
  net::test_server::EmbeddedTestServer https_server_;
  base::test::ScopedFeatureList scoped_feature_list_;
  PolicyMap policies_;
};

// Generates test names containing params for easier debugging.
std::string TestNameGenerator(
    const testing::TestParamInfo<EnabledPolicyBrowsertest::ParamType>& info) {
  std::string name = base::NumberToString(info.index);

  auto policy_state_to_string =
      [](PolicyTest::BooleanPolicy state) -> std::string {
    switch (state) {
      case PolicyTest::BooleanPolicy::kNotConfigured:
        return "NotConfigured";
      case PolicyTest::BooleanPolicy::kFalse:
        return "False";
      case PolicyTest::BooleanPolicy::kTrue:
        return "True";
    }
  };

  PolicyTest::BooleanPolicy first_party_sets_policy_state =
      std::get<0>(info.param);
  base::StrAppend(&name,
                  {"_", policy_state_to_string(first_party_sets_policy_state)});

  PolicyTest::BooleanPolicy related_website_sets_policy_state =
      std::get<1>(info.param);
  base::StrAppend(
      &name, {"_", policy_state_to_string(related_website_sets_policy_state)});

  const char* override_policy_name = std::get<2>(info.param);
  base::StrAppend(&name, {"_", override_policy_name});

  return name;
}

IN_PROC_BROWSER_TEST_P(EnabledPolicyBrowsertest, ToggleFeature_Memberships) {
  EXPECT_EQ(IsPrefEnabledInitially(),
            AreSitesInSameRelatedWebsiteSet(kHostA, kHostC));
  EXPECT_EQ(IsPrefEnabledInitially(),
            AreSitesInSameRelatedWebsiteSet(kHostA, kHostB));

  SetEnabledPolicyStates(!IsPrefEnabledInitially());

  EXPECT_EQ(!IsPrefEnabledInitially(),
            AreSitesInSameRelatedWebsiteSet(kHostA, kHostC));
  EXPECT_EQ(!IsPrefEnabledInitially(),
            AreSitesInSameRelatedWebsiteSet(kHostA, kHostB));
}

IN_PROC_BROWSER_TEST_P(EnabledPolicyBrowsertest, ToggleFeature_NonMemberships) {
  EXPECT_FALSE(AreSitesInSameRelatedWebsiteSet(kHostD, kHostA));
  SetEnabledPolicyStates(!IsPrefEnabledInitially());

  EXPECT_FALSE(AreSitesInSameRelatedWebsiteSet(kHostD, kHostA));
}

INSTANTIATE_TEST_SUITE_P(
    FirstPartySets,
    EnabledPolicyBrowsertest,
    ::testing::Combine(
        ::testing::Values(
            PolicyTest::BooleanPolicy::kNotConfigured,
            PolicyTest::BooleanPolicy::kFalse,
            PolicyTest::BooleanPolicy::kTrue),  // FirstPartySetsEnabled
        ::testing::Values(
            PolicyTest::BooleanPolicy::kNotConfigured,
            PolicyTest::BooleanPolicy::kFalse,
            PolicyTest::BooleanPolicy::kTrue),  // RelatedWebsiteSetsEnabled
        ::testing::Values(
            policy::key::kFirstPartySetsOverrides,
            policy::key::kRelatedWebsiteSetsOverrides)  // Overrides Policy
        ),
    TestNameGenerator);

class OverridesPolicyEmptyBrowsertest : public EnabledPolicyBrowsertest {
 public:
  std::optional<std::string> GetOverridesPolicy() override { return R"( {} )"; }
};

IN_PROC_BROWSER_TEST_P(OverridesPolicyEmptyBrowsertest, CheckMemberships) {
  // The initial Related Website Sets were:
  // {primary: A, associatedSites: [B, C]}
  //
  // After the Overrides policy is applied, the expected Related Website Sets
  // are: {primary: A, associatedSites: [B, C]} (unchanged)
  EXPECT_EQ(IsRelatedWebsiteSetsEnabledInitially(),
            AreSitesInSameRelatedWebsiteSet(kHostC, kHostA));
  EXPECT_EQ(IsRelatedWebsiteSetsEnabledInitially(),
            AreSitesInSameRelatedWebsiteSet(kHostB, kHostA));
}

INSTANTIATE_TEST_SUITE_P(
    FirstPartySets,
    OverridesPolicyEmptyBrowsertest,
    ::testing::Combine(
        ::testing::Values(
            PolicyTest::BooleanPolicy::kNotConfigured,
            PolicyTest::BooleanPolicy::kFalse,
            PolicyTest::BooleanPolicy::kTrue),  // FirstPartySetsEnabled
        ::testing::Values(
            PolicyTest::BooleanPolicy::kNotConfigured,
            PolicyTest::BooleanPolicy::kFalse,
            PolicyTest::BooleanPolicy::kTrue),  // RelatedWebsiteSetsEnabled
        ::testing::Values(
            policy::key::kFirstPartySetsOverrides,
            policy::key::kRelatedWebsiteSetsOverrides)  // Overrides Policy
        ),
    TestNameGenerator);

class OverridesPolicyReplacementBrowsertest : public EnabledPolicyBrowsertest {
 public:
  std::optional<std::string> GetOverridesPolicy() override {
    return R"(
        {
          "replacements": [
            {
              "primary": "https://d.test",
              "associatedSites": ["https://b.test", "https://a.test"]
            }
          ],
          "additions": []
        }
      )";
  }
};

IN_PROC_BROWSER_TEST_P(OverridesPolicyReplacementBrowsertest,
                       CheckMemberships) {
  // The initial Related Website Sets were:
  // {primary: A, associatedSites: [B, C]}
  //
  // After the Overrides policy is applied, the expected Related Website Sets
  // are: {primary: D, associatedSites: [A, B]}
  EXPECT_EQ(IsRelatedWebsiteSetsEnabledInitially(),
            AreSitesInSameRelatedWebsiteSet(kHostA, kHostB));
  EXPECT_EQ(IsRelatedWebsiteSetsEnabledInitially(),
            AreSitesInSameRelatedWebsiteSet(kHostA, kHostD));
  EXPECT_FALSE(AreSitesInSameRelatedWebsiteSet(kHostA, kHostC));
}

INSTANTIATE_TEST_SUITE_P(
    FirstPartySets,
    OverridesPolicyReplacementBrowsertest,
    ::testing::Combine(
        ::testing::Values(
            PolicyTest::BooleanPolicy::kNotConfigured,
            PolicyTest::BooleanPolicy::kFalse,
            PolicyTest::BooleanPolicy::kTrue),  // FirstPartySetsEnabled
        ::testing::Values(
            PolicyTest::BooleanPolicy::kNotConfigured,
            PolicyTest::BooleanPolicy::kFalse,
            PolicyTest::BooleanPolicy::kTrue),  // RelatedWebsiteSetsEnabled
        ::testing::Values(
            policy::key::kFirstPartySetsOverrides,
            policy::key::kRelatedWebsiteSetsOverrides)  // Overrides Policy
        ),
    TestNameGenerator);

class OverridesPolicyAdditionBrowsertest : public EnabledPolicyBrowsertest {
 public:
  std::optional<std::string> GetOverridesPolicy() override {
    return R"(
        {
          "replacements": [],
          "additions": [
            {
              "primary": "https://a.test",
              "associatedSites": ["https://d.test"]
            }
          ]
        }
      )";
  }
};

IN_PROC_BROWSER_TEST_P(OverridesPolicyAdditionBrowsertest, CheckMemberships) {
  // The initial Related Website Sets were:
  // {primary: A, associatedSites: [B, C]}
  //
  // After the Overrides policy is applied, the expected Related Website Sets
  // are: {primary: A, associatedSites: [B, C, D]}}
  EXPECT_EQ(IsRelatedWebsiteSetsEnabledInitially(),
            AreSitesInSameRelatedWebsiteSet(kHostA, kHostD));
  EXPECT_EQ(IsRelatedWebsiteSetsEnabledInitially(),
            AreSitesInSameRelatedWebsiteSet(kHostA, kHostB));
  EXPECT_EQ(IsRelatedWebsiteSetsEnabledInitially(),
            AreSitesInSameRelatedWebsiteSet(kHostA, kHostC));
}

INSTANTIATE_TEST_SUITE_P(
    FirstPartySets,
    OverridesPolicyAdditionBrowsertest,
    ::testing::Combine(
        ::testing::Values(
            PolicyTest::BooleanPolicy::kNotConfigured,
            PolicyTest::BooleanPolicy::kFalse,
            PolicyTest::BooleanPolicy::kTrue),  // FirstPartySetsEnabled
        ::testing::Values(
            PolicyTest::BooleanPolicy::kNotConfigured,
            PolicyTest::BooleanPolicy::kFalse,
            PolicyTest::BooleanPolicy::kTrue),  // RelatedWebsiteSetsEnabled
        ::testing::Values(
            policy::key::kFirstPartySetsOverrides,
            policy::key::kRelatedWebsiteSetsOverrides)  // Overrides Policy
        ),
    TestNameGenerator);

class OverridesPolicyReplacementAndAdditionBrowsertest
    : public EnabledPolicyBrowsertest {
 public:
  std::optional<std::string> GetOverridesPolicy() override {
    return R"(
        {
          "replacements": [
            {
              "primary": "https://a.test",
              "associatedSites": ["https://d.test"]
            }
          ],
          "additions": [
            {
              "primary": "https://b.test",
              "associatedSites": ["https://c.test"]
            }
          ]
        }
      )";
  }
};

IN_PROC_BROWSER_TEST_P(OverridesPolicyReplacementAndAdditionBrowsertest,
                       CheckMemberships) {
  // The initial Related Website Sets were:
  // {primary: A, associatedSites: [B, C]}
  //
  // After the Overrides policy is applied, the expected Related Website Sets
  // are:
  // {primary: A, associatedSites: [D]} and {primary: B, associatedSites: [C]}.
  EXPECT_FALSE(AreSitesInSameRelatedWebsiteSet(kHostB, kHostA));
  EXPECT_EQ(IsRelatedWebsiteSetsEnabledInitially(),
            AreSitesInSameRelatedWebsiteSet(kHostD, kHostA));
  EXPECT_EQ(IsRelatedWebsiteSetsEnabledInitially(),
            AreSitesInSameRelatedWebsiteSet(kHostC, kHostB));
}

INSTANTIATE_TEST_SUITE_P(
    FirstPartySets,
    OverridesPolicyReplacementAndAdditionBrowsertest,
    ::testing::Combine(
        ::testing::Values(
            PolicyTest::BooleanPolicy::kNotConfigured,
            PolicyTest::BooleanPolicy::kFalse,
            PolicyTest::BooleanPolicy::kTrue),  // FirstPartySetsEnabled
        ::testing::Values(
            PolicyTest::BooleanPolicy::kNotConfigured,
            PolicyTest::BooleanPolicy::kFalse,
            PolicyTest::BooleanPolicy::kTrue),  // RelatedWebsiteSetsEnabled
        ::testing::Values(
            policy::key::kFirstPartySetsOverrides,
            policy::key::kRelatedWebsiteSetsOverrides)  // Overrides Policy
        ),
    TestNameGenerator);
}  // namespace
}  // namespace policy
