// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>

#include "base/json/json_reader.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
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

enum class PrefState { kDefault, kDisabled, kEnabled };

const char* kHostA = "a.test";
const char* kHostB = "b.test";
const char* kHostC = "c.test";
const char* kHostD = "d.test";

class EnabledPolicyBrowsertest
    : public PolicyTest,
      public ::testing::WithParamInterface<std::tuple<bool, PrefState>> {
 public:
  EnabledPolicyBrowsertest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    std::vector<base::test::FeatureRef> enabled_features = {
        blink::features::kStorageAccessAPI};
    std::vector<base::test::FeatureRef> disabled_features;
    if (IsFeatureEnabled()) {
      enabled_features.emplace_back(features::kFirstPartySets);
    } else {
      disabled_features.emplace_back(features::kFirstPartySets);
    }
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
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
    if (absl::optional<std::string> policy = GetOverridesPolicy();
        policy.has_value()) {
      SetPolicyValue(policy::key::kFirstPartySetsOverrides,
                     base::JSONReader::Read(policy.value()));
    }
    if (GetPrefState() != PrefState::kDefault) {
      SetPolicyValue(policy::key::kFirstPartySetsEnabled,
                     base::Value(GetPrefState() == PrefState::kEnabled));
    }

    provider_.UpdateChromePolicy(policies_);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    PolicyTest::SetUpCommandLine(command_line);
    if (IsFeatureEnabled()) {
      // Only append this switch when the First-Party Sets base::Feature is
      // enabled.
      command_line->AppendSwitchASCII(
          network::switches::kUseFirstPartySet,
          base::StringPrintf(
              R"({"primary": "https://%s",)"
              R"("associatedSites": ["https://%s","https://%s"]})",
              kHostA, kHostB, kHostC));
    }
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
  virtual absl::optional<std::string> GetOverridesPolicy() {
    return absl::nullopt;
  }

  // Sets the state of the First-Party Sets enabled preference.
  void SetEnabledPolicyState(bool enabled) {
    SetPolicyValue(policy::key::kFirstPartySetsEnabled, base::Value(enabled));

    provider_.UpdateChromePolicy(policies_);
  }

  // Returns whether or not First-Party Sets was enabled at the start of the
  // test. This does not account for calls to `SetEnabledPolicyState`.
  bool IsFirstPartySetsEnabledInitially() {
    return IsFeatureEnabled() && GetPrefState() != PrefState::kDisabled;
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

  bool AreSitesInSameFirstPartySet(const std::string& first_host,
                                   const std::string& second_host) {
    NavigateToPageWithFrame(first_host);
    NavigateFrameTo(https_server_.GetURL(second_host, "/empty.html"));

    return content::ExecJs(GetFrame(), "document.requestStorageAccess()");
  }

  bool IsFeatureEnabled() { return std::get<0>(GetParam()); }
  PrefState GetPrefState() { return std::get<1>(GetParam()); }

 private:
  void SetPolicyValue(const char* key, absl::optional<base::Value> value) {
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

IN_PROC_BROWSER_TEST_P(EnabledPolicyBrowsertest, ToggleFeature_Memberships) {
  const bool feature_enabled = IsFeatureEnabled();
  const bool pref_initially_enabled = GetPrefState() != PrefState::kDisabled;

  EXPECT_EQ(feature_enabled && pref_initially_enabled,
            AreSitesInSameFirstPartySet(kHostA, kHostC));
  EXPECT_EQ(feature_enabled && pref_initially_enabled,
            AreSitesInSameFirstPartySet(kHostA, kHostB));

  SetEnabledPolicyState(!pref_initially_enabled);

  EXPECT_EQ(feature_enabled && !pref_initially_enabled,
            AreSitesInSameFirstPartySet(kHostA, kHostC));
  EXPECT_EQ(feature_enabled && !pref_initially_enabled,
            AreSitesInSameFirstPartySet(kHostA, kHostB));
}

IN_PROC_BROWSER_TEST_P(EnabledPolicyBrowsertest, ToggleFeature_NonMemberships) {
  EXPECT_FALSE(AreSitesInSameFirstPartySet(kHostD, kHostA));
  const bool pref_initially_enabled = GetPrefState() != PrefState::kDisabled;
  SetEnabledPolicyState(!pref_initially_enabled);

  EXPECT_FALSE(AreSitesInSameFirstPartySet(kHostD, kHostA));
}

INSTANTIATE_TEST_SUITE_P(
    FirstPartySets,
    EnabledPolicyBrowsertest,
    ::testing::Combine(::testing::Bool(),
                       ::testing::Values(PrefState::kDefault,
                                         PrefState::kDisabled,
                                         PrefState::kEnabled)));

class OverridesPolicyEmptyBrowsertest : public EnabledPolicyBrowsertest {
 public:
  absl::optional<std::string> GetOverridesPolicy() override {
    return R"( {} )";
  }
};

IN_PROC_BROWSER_TEST_P(OverridesPolicyEmptyBrowsertest, CheckMemberships) {
  // The initial First-Party Sets were:
  // {primary: A, associatedSites: [B, C]}
  //
  // After the Overrides policy is applied, the expected First-Party Sets are:
  // {primary: A, associatedSites: [B, C]} (unchanged)
  EXPECT_EQ(IsFirstPartySetsEnabledInitially(),
            AreSitesInSameFirstPartySet(kHostC, kHostA));
  EXPECT_EQ(IsFirstPartySetsEnabledInitially(),
            AreSitesInSameFirstPartySet(kHostB, kHostA));
}

INSTANTIATE_TEST_SUITE_P(
    FirstPartySets,
    OverridesPolicyEmptyBrowsertest,
    ::testing::Combine(::testing::Bool(),
                       ::testing::Values(PrefState::kDefault,
                                         PrefState::kDisabled,
                                         PrefState::kEnabled)));

class OverridesPolicyReplacementBrowsertest : public EnabledPolicyBrowsertest {
 public:
  absl::optional<std::string> GetOverridesPolicy() override {
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
  // The initial First-Party Sets were:
  // {primary: A, associatedSites: [B, C]}
  //
  // After the Overrides policy is applied, the expected First-Party Sets are:
  // {primary: D, associatedSites: [A, B]}
  EXPECT_EQ(IsFirstPartySetsEnabledInitially(),
            AreSitesInSameFirstPartySet(kHostA, kHostB));
  EXPECT_EQ(IsFirstPartySetsEnabledInitially(),
            AreSitesInSameFirstPartySet(kHostA, kHostD));
  EXPECT_FALSE(AreSitesInSameFirstPartySet(kHostA, kHostC));
}

INSTANTIATE_TEST_SUITE_P(
    FirstPartySets,
    OverridesPolicyReplacementBrowsertest,
    ::testing::Combine(::testing::Bool(),
                       ::testing::Values(PrefState::kDefault,
                                         PrefState::kDisabled,
                                         PrefState::kEnabled)));

class OverridesPolicyAdditionBrowsertest : public EnabledPolicyBrowsertest {
 public:
  absl::optional<std::string> GetOverridesPolicy() override {
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
  // The initial First-Party Sets were:
  // {primary: A, associatedSites: [B, C]}
  //
  // After the Overrides policy is applied, the expected First-Party Sets are:
  // {primary: A, associatedSites: [B, C, D]}}
  EXPECT_EQ(IsFirstPartySetsEnabledInitially(),
            AreSitesInSameFirstPartySet(kHostA, kHostD));
  EXPECT_EQ(IsFirstPartySetsEnabledInitially(),
            AreSitesInSameFirstPartySet(kHostA, kHostB));
  EXPECT_EQ(IsFirstPartySetsEnabledInitially(),
            AreSitesInSameFirstPartySet(kHostA, kHostC));
}

INSTANTIATE_TEST_SUITE_P(
    FirstPartySets,
    OverridesPolicyAdditionBrowsertest,
    ::testing::Combine(::testing::Bool(),
                       ::testing::Values(PrefState::kDefault,
                                         PrefState::kDisabled,
                                         PrefState::kEnabled)));

class OverridesPolicyReplacementAndAdditionBrowsertest
    : public EnabledPolicyBrowsertest {
 public:
  absl::optional<std::string> GetOverridesPolicy() override {
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
  // The initial First-Party Sets were:
  // {primary: A, associatedSites: [B, C]}
  //
  // After the Overrides policy is applied, the expected First-Party Sets are:
  // {primary: A, associatedSites: [D]} and {primary: B, associatedSites: [C]}.
  EXPECT_FALSE(AreSitesInSameFirstPartySet(kHostB, kHostA));
  EXPECT_EQ(IsFirstPartySetsEnabledInitially(),
            AreSitesInSameFirstPartySet(kHostD, kHostA));
  EXPECT_EQ(IsFirstPartySetsEnabledInitially(),
            AreSitesInSameFirstPartySet(kHostC, kHostB));
}

INSTANTIATE_TEST_SUITE_P(
    FirstPartySets,
    OverridesPolicyReplacementAndAdditionBrowsertest,
    ::testing::Combine(::testing::Bool(),
                       ::testing::Values(PrefState::kDefault,
                                         PrefState::kDisabled,
                                         PrefState::kEnabled)));
}  // namespace
}  // namespace policy
