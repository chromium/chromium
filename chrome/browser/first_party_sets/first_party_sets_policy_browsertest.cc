// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>

#include "base/json/json_reader.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/frame_test_utils.h"
#include "net/base/features.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/canonical_cookie_test_helpers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/network_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {
namespace {

using ::testing::UnorderedPointwise;

enum PrefState { kDefault, kDisabled, kEnabled };

const char* kHostA = "a.test";
const char* kHostB = "b.test";
const char* kHostC = "c.test";
const char* kHostD = "d.test";
const char* kSamePartyLaxCookieName = "sameparty_lax_cookie";
const char* kSamePartyNoneCookieName = "sameparty_none_cookie";
const char* kSamePartyUnspecifiedCookieName = "sameparty_unspecified_cookie";
const std::string kSetSamePartyCookiesURL = base::StrCat({
    "/set-cookie?",
    kSamePartyLaxCookieName,
    "=1;SameParty;Secure;SameSite=Lax&",
    kSamePartyNoneCookieName,
    "=1;SameParty;Secure;SameSite=None&",
    kSamePartyUnspecifiedCookieName,
    "=1;SameParty;Secure",
});
const std::vector<std::string> kAllCookies = {kSamePartyLaxCookieName,
                                              kSamePartyNoneCookieName,
                                              kSamePartyUnspecifiedCookieName};

class EnabledPolicyBrowsertest
    : public PolicyTest,
      public ::testing::WithParamInterface<std::tuple<bool, PrefState>> {
 public:
  EnabledPolicyBrowsertest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS),
        pref_enabled_(GetPrefState() != PrefState::kDisabled) {
    if (IsFeatureEnabled()) {
      scoped_feature_list_.InitWithFeatures(
          {features::kFirstPartySets,
           net::features::kSamePartyAttributeEnabled},
          {});
    } else {
      scoped_feature_list_.InitAndDisableFeature(features::kFirstPartySets);
    }
  }

  void SetUpOnMainThread() override {
    PolicyTest::SetUpOnMainThread();
    // Add content/test/data for cross_site_iframe_factory.html
    https_server()->ServeFilesFromSourceDirectory("content/test/data");
    https_server()->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server()->AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(https_server()->Start());
  }

  void SetUpInProcessBrowserTestFixture() override {
    PolicyTest::SetUpInProcessBrowserTestFixture();
    SetUpPolicyMapWithOverridesPolicy();
    // POLICY_LEVEL_MANDATORY - since administrators will control FPS policy
    // POLICY_SCOPE_USER - since this policy is per profile, not on local state
    // POLICY_SOURCE_ENTERPRISE_DEFAULT - since this is an enterprise policy
    if (GetPrefState() != PrefState::kDefault) {
      policies_.Set(policy::key::kFirstPartySetsEnabled, POLICY_LEVEL_MANDATORY,
                    POLICY_SCOPE_USER, POLICY_SOURCE_ENTERPRISE_DEFAULT,
                    base::Value(IsPrefEnabled()), nullptr);
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

 protected:
  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  GURL SetSamePartyCookiesUrl(const std::string& host) {
    return https_server()->GetURL(host, kSetSamePartyCookiesURL);
  }

  virtual void SetUpPolicyMapWithOverridesPolicy() {}

  net::EmbeddedTestServer* https_server() { return &https_server_; }
  PolicyMap& policy_map() { return policies_; }

  // Reverses the state of the First-Party Sets enabled preference.
  void FlipEnabledPolicy() {
    pref_enabled_ = !pref_enabled_;
    policy_map().Set(policy::key::kFirstPartySetsEnabled,
                     POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                     POLICY_SOURCE_ENTERPRISE_DEFAULT,
                     base::Value(pref_enabled_), nullptr);

    provider_.UpdateChromePolicy(policy_map());
  }

  bool IsFirstPartySetsEnabled() {
    return IsFeatureEnabled() && IsPrefEnabled();
  }

  // Clear cookies for the current browser context, returning the number
  // cleared.
  uint32_t ClearCookies() {
    return content::DeleteCookies(web_contents()->GetBrowserContext(),
                                  network::mojom::CookieDeletionFilter());
  }

  bool AreSitesInSameFirstPartySet(const std::string& first_host,
                                   const std::string& second_host) {
    std::vector<net::CanonicalCookie> cookies =
        ArrangeFramesAndGetCanonicalCookiesForLeaf(
            web_contents(), https_server(),
            base::StringPrintf("%s(%%s)", first_host.c_str()),
            SetSamePartyCookiesUrl(second_host));
    ClearCookies();
    return testing::Value(
        cookies, UnorderedPointwise(net::CanonicalCookieNameIs(), kAllCookies));
  }

 private:
  bool IsFeatureEnabled() { return std::get<0>(GetParam()); }
  PrefState GetPrefState() { return std::get<1>(GetParam()); }
  bool IsPrefEnabled() { return pref_enabled_; }

  net::test_server::EmbeddedTestServer https_server_;
  base::test::ScopedFeatureList scoped_feature_list_;
  PolicyMap policies_;
  bool pref_enabled_;
};

IN_PROC_BROWSER_TEST_P(
    EnabledPolicyBrowsertest,
    ToggleFeature_Memberships) {
  EXPECT_EQ(IsFirstPartySetsEnabled(),
            AreSitesInSameFirstPartySet(kHostA, kHostC));
  EXPECT_EQ(IsFirstPartySetsEnabled(),
            AreSitesInSameFirstPartySet(kHostA, kHostB));

  FlipEnabledPolicy();

  EXPECT_EQ(IsFirstPartySetsEnabled(),
            AreSitesInSameFirstPartySet(kHostA, kHostC));
  EXPECT_EQ(IsFirstPartySetsEnabled(),
            AreSitesInSameFirstPartySet(kHostA, kHostB));
}

IN_PROC_BROWSER_TEST_P(EnabledPolicyBrowsertest,
                        ToggleFeature_NonMemberships) {
  EXPECT_FALSE(AreSitesInSameFirstPartySet(kHostD, kHostA));

  FlipEnabledPolicy();

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
  void SetUpPolicyMapWithOverridesPolicy() override {
    // POLICY_LEVEL_MANDATORY - since administrators will control FPS policy
    // POLICY_SCOPE_USER - since this policy is per profile, not on local state
    // POLICY_SOURCE_ENTERPRISE_DEFAULT - since this is an enterprise
    // policy
    policy_map().Set(policy::key::kFirstPartySetsOverrides,
                     POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                     POLICY_SOURCE_ENTERPRISE_DEFAULT,
                     base::JSONReader::Read(R"( {} )"), nullptr);
  }
};

IN_PROC_BROWSER_TEST_P(OverridesPolicyEmptyBrowsertest,
                       CheckMemberships) {
  // The initial First-Party Sets were:
  // {primary: A, associatedSites: [B, C]}
  //
  // After the Overrides policy is applied, the expected First-Party Sets are:
  // {primary: A, associatedSites: [B, C]} (unchanged)
  EXPECT_EQ(IsFirstPartySetsEnabled(),
            AreSitesInSameFirstPartySet(kHostC, kHostA));
  EXPECT_EQ(IsFirstPartySetsEnabled(),
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
  void SetUpPolicyMapWithOverridesPolicy() override {
    // POLICY_LEVEL_MANDATORY - since administrators will control FPS policy
    // POLICY_SCOPE_USER - since this policy is per profile, not on local state
    // POLICY_SOURCE_ENTERPRISE_DEFAULT - since this is an enterprise
    // policy
    policy_map().Set(policy::key::kFirstPartySetsOverrides,
                     POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                     POLICY_SOURCE_ENTERPRISE_DEFAULT,
                     base::JSONReader::Read(R"(
                              {
                                "replacements": [
                                  {
                                    "primary": "https://d.test",
                                    "associatedSites": ["https://b.test",
                                    "https://a.test"]
                                  }
                                ],
                                "additions": []
                              }
                            )"),
                     nullptr);
  }
};

IN_PROC_BROWSER_TEST_P(OverridesPolicyReplacementBrowsertest,
                       CheckMemberships) {
  // The initial First-Party Sets were:
  // {primary: A, associatedSites: [B, C]}
  //
  // After the Overrides policy is applied, the expected First-Party Sets are:
  // {primary: D, associatedSites: [A, B]}
  EXPECT_EQ(IsFirstPartySetsEnabled(),
            AreSitesInSameFirstPartySet(kHostA, kHostB));
  EXPECT_EQ(IsFirstPartySetsEnabled(),
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
  void SetUpPolicyMapWithOverridesPolicy() override {
    // POLICY_LEVEL_MANDATORY - since administrators will control FPS policy
    // POLICY_SCOPE_USER - since this policy is per profile, not on local state
    // POLICY_SOURCE_ENTERPRISE_DEFAULT - since this is an enterprise
    // policy
    policy_map().Set(policy::key::kFirstPartySetsOverrides,
                     POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                     POLICY_SOURCE_ENTERPRISE_DEFAULT,
                     base::JSONReader::Read(R"(
                              {
                                "replacements": [],
                                "additions": [
                                  {
                                    "primary": "https://a.test",
                                    "associatedSites": ["https://d.test"]
                                  }
                                ]
                              }
                            )"),
                     nullptr);
  }
};

IN_PROC_BROWSER_TEST_P(OverridesPolicyAdditionBrowsertest,
                       CheckMemberships) {
  // The initial First-Party Sets were:
  // {primary: A, associatedSites: [B, C]}
  //
  // After the Overrides policy is applied, the expected First-Party Sets are:
  // {primary: A, associatedSites: [B, C, D]}}
  EXPECT_EQ(IsFirstPartySetsEnabled(),
            AreSitesInSameFirstPartySet(kHostA, kHostD));
  EXPECT_EQ(IsFirstPartySetsEnabled(),
            AreSitesInSameFirstPartySet(kHostA, kHostB));
  EXPECT_EQ(IsFirstPartySetsEnabled(),
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
  void SetUpPolicyMapWithOverridesPolicy() override {
    // POLICY_LEVEL_MANDATORY - since administrators will control FPS policy
    // POLICY_SCOPE_USER - since this policy is per profile, not on local state
    // POLICY_SOURCE_ENTERPRISE_DEFAULT - since this is an enterprise
    // policy
    policy_map().Set(policy::key::kFirstPartySetsOverrides,
                     POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                     POLICY_SOURCE_ENTERPRISE_DEFAULT,
                     base::JSONReader::Read(R"(
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
                            )"),
                     nullptr);
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
  EXPECT_EQ(IsFirstPartySetsEnabled(),
            AreSitesInSameFirstPartySet(kHostD, kHostA));
  EXPECT_EQ(IsFirstPartySetsEnabled(),
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