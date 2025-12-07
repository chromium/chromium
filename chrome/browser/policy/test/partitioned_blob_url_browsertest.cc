// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "storage/browser/blob/features.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/switches.h"
#include "url/gurl.h"

namespace policy {

namespace {
GURL CreateBlobUrl(content::RenderFrameHost* rfh) {
  std::string blob_url_string =
      EvalJs(rfh,
             "const blob_url = URL.createObjectURL(new "
             "  Blob(['<!doctype html><body>potato</body>'], {type: "
             "  'text/html'}));"
             "blob_url;")
          .ExtractString();
  return GURL(blob_url_string);
}

bool FetchAndReadBlobUrl(content::RenderFrameHost* rfh, const GURL& blob_url) {
  return content::EvalJs(rfh,
                         content::JsReplace(
                             "async function test() {"
                             "  try {"
                             "    const blob = await "
                             "    fetch($1).then(response => response.blob());"
                             "    await blob.text();"
                             "    return true;"
                             "  } catch (e) {"
                             "    return false;"
                             "  }"
                             "}"
                             "test();",
                             blob_url))
      .ExtractBool();
}
}  // namespace
class PartitionedBlobUrlUsagePolicyBrowserTest : public policy::PolicyTest {
 public:
  PartitionedBlobUrlUsagePolicyBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kBlockCrossPartitionBlobUrlFetching,
         blink::features::kEnforceNoopenerOnBlobURLNavigation},
        {});
  }
  void SetUpOnMainThread() override {
    PolicyTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");

    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
    content::SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void SetEnterprisePolicies(int enabled_value,
                             const std::vector<std::string>& origins) {
    policy::PolicyMap policies;

    policies.Set(policy::key::kPartitionedBlobUrlUsage,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, base::Value(true), nullptr);
    policies.Set(policy::key::kDefaultThirdPartyStoragePartitioningSetting,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, base::Value(enabled_value),
                 nullptr);

    base::Value::List blocked_origins;
    for (const auto& origin : origins) {
      blocked_origins.Append(origin);
    }
    policies.Set(policy::key::kThirdPartyStoragePartitioningBlockedForOrigins,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD,
                 base::Value(std::move(blocked_origins)), nullptr);

    UpdateProviderPolicy(policies);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// When blob URL partitioning is enabled via feature flags and its enterprise
// policy, partitioning can still be disabled by the overarching third-party
// storage partitioning enterprise policies.
IN_PROC_BROWSER_TEST_F(
    PartitionedBlobUrlUsagePolicyBrowserTest,
    FetchingTestWithDefaultThirdPartyStoragePartitioningSetting) {
  EXPECT_TRUE(base::FeatureList::IsEnabled(
      features::kBlockCrossPartitionBlobUrlFetching));

  SetEnterprisePolicies(ContentSetting::CONTENT_SETTING_BLOCK, {});
  GURL main_url = embedded_test_server()->GetURL(
      "c.com", "/cross_site_iframe_factory.html?c(b(c))");

  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);

  EXPECT_TRUE(content::NavigateToURL(web_contents, main_url));
  content::RenderFrameHost* rfh_c = web_contents->GetPrimaryMainFrame();

  GURL blob_url(CreateBlobUrl(rfh_c));

  content::RenderFrameHost* rfh_b = content::ChildFrameAt(rfh_c, 0);
  content::RenderFrameHost* rfh_c_2 = content::ChildFrameAt(rfh_b, 0);

  EXPECT_TRUE(FetchAndReadBlobUrl(rfh_c_2, blob_url));
}

IN_PROC_BROWSER_TEST_F(
    PartitionedBlobUrlUsagePolicyBrowserTest,
    FetchingTestWithThirdPartyStoragePartitioningBlockedForOrigins) {
  GURL main_url = embedded_test_server()->GetURL(
      "c.com", "/cross_site_iframe_factory.html?c(b(c))");

  SetEnterprisePolicies(
      ContentSetting::CONTENT_SETTING_ALLOW, /*3PSP disabled for*/
      {url::Origin::Create(main_url).Serialize()});

  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);

  EXPECT_TRUE(content::NavigateToURL(web_contents, main_url));
  content::RenderFrameHost* rfh_c = web_contents->GetPrimaryMainFrame();

  GURL blob_url(CreateBlobUrl(rfh_c));

  content::RenderFrameHost* rfh_b = content::ChildFrameAt(rfh_c, 0);
  content::RenderFrameHost* rfh_c_2 = content::ChildFrameAt(rfh_b, 0);

  EXPECT_TRUE(FetchAndReadBlobUrl(rfh_c_2, blob_url));
}

class PartitionedBlobUrlUsagePolicyBrowserTestP
    : public policy::PolicyTest,
      public testing::WithParamInterface<bool> {
 public:
  PartitionedBlobUrlUsagePolicyBrowserTestP() {
    scoped_feature_list_.InitWithFeatures(
        {features::kBlockCrossPartitionBlobUrlFetching,
         blink::features::kEnforceNoopenerOnBlobURLNavigation},
        {});
  }
  void SetUpOnMainThread() override {
    PolicyTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");

    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
    content::SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void SetUpInProcessBrowserTestFixture() override {
    PolicyTest::SetUpInProcessBrowserTestFixture();
    // The policy must be set in this function because if the policy is set in
    // the test body the test fails because it seems that an existing renderer
    // is re-used for which the command line flag hasn't been set.
    SetEnterprisePolicies(GetParam());
  }

  void SetEnterprisePolicies(bool enabled) {
    policy::PolicyMap policies;
    policies.Set(policy::key::kPartitionedBlobUrlUsage,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                 policy::POLICY_SOURCE_CLOUD, base::Value(enabled), nullptr);

    UpdateProviderPolicy(policies);
  }

  bool IsPolicyEnabled() { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(PartitionedBlobUrlUsagePolicyBrowserTestP,
                       FetchingPartitionedBlobUrlUsageEnterprisePolicy) {
  EXPECT_TRUE(base::FeatureList::IsEnabled(
      features::kBlockCrossPartitionBlobUrlFetching));

  GURL main_url = embedded_test_server()->GetURL(
      "c.com", "/cross_site_iframe_factory.html?c(b(c))");

  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);

  EXPECT_TRUE(content::NavigateToURL(web_contents, main_url));
  content::RenderFrameHost* rfh_c = web_contents->GetPrimaryMainFrame();

  GURL blob_url(CreateBlobUrl(rfh_c));

  content::RenderFrameHost* rfh_b = content::ChildFrameAt(rfh_c, 0);
  content::RenderFrameHost* rfh_c_2 = content::ChildFrameAt(rfh_b, 0);

  // The default cookie setting to BLOCK here to ensure that the
  // cross-origin blob URL fetch will be blocked due to lack of storage
  // access. If cookies are allowed, storage access might be granted, and the
  // fetch would succeed even if the blob URL is cross-origin and
  // kBlockCrossPartitionBlobUrlFetching is enabled.
  content_settings::CookieSettings* settings =
      CookieSettingsFactory::GetForProfile(browser()->profile()).get();
  settings->SetDefaultCookieSetting(CONTENT_SETTING_BLOCK);

  bool fetch_results = FetchAndReadBlobUrl(rfh_c_2, blob_url);

  if (IsPolicyEnabled()) {
    EXPECT_FALSE(fetch_results);
  } else {
    EXPECT_TRUE(fetch_results);
  }
}

IN_PROC_BROWSER_TEST_P(
    PartitionedBlobUrlUsagePolicyBrowserTestP,
    EnforcingNoopenerOnPartitionedBlobUrlUsageEnterprisePolicy) {
  EXPECT_TRUE(base::FeatureList::IsEnabled(
      blink::features::kEnforceNoopenerOnBlobURLNavigation));
  // 1. navigate to c.com
  GURL main_url = embedded_test_server()->GetURL("c.com", "/title1.html");

  content::WebContents* web_contents =
      chrome_test_utils::GetActiveWebContents(this);
  EXPECT_TRUE(content::NavigateToURL(web_contents, main_url));
  content::RenderFrameHost* rfh_c = web_contents->GetPrimaryMainFrame();

  // 2. create blob_url in c.com (blob url origin is c.com)
  GURL blob_url(CreateBlobUrl(rfh_c));

  // 3. window.open b.com with c.com embedded
  // 3a. navigate to b.com
  GURL b_url = embedded_test_server()->GetURL(
      "b.com", "/cross_site_iframe_factory.html?b(c)");

  // 3b. open new tab from b.com context
  ui_test_utils::TabAddedWaiter tab_waiter(browser());
  EXPECT_TRUE(
      content::ExecJs(rfh_c, content::JsReplace("window.open($1)", b_url)));
  tab_waiter.Wait();
  content::WebContents* tab_b =
      browser()->tab_strip_model()->GetWebContentsAt(1);
  ASSERT_NE(nullptr, tab_b) << "No tab found at index 1.";

  content::TestNavigationObserver nav_observer(tab_b);
  nav_observer.Wait();

  content::RenderFrameHost* rfh_b = tab_b->GetPrimaryMainFrame();
  content::RenderFrameHost* rfh_c_in_b = content::ChildFrameAt(rfh_b, 0);

  // 4. do the window.open of blob url from c.com
  ui_test_utils::TabAddedWaiter tab_waiter2(browser());
  EXPECT_TRUE(rfh_c_in_b->IsRenderFrameLive());
  EXPECT_TRUE(content::ExecJs(
      rfh_c_in_b, content::JsReplace("handle = window.open($1);", blob_url)));
  tab_waiter2.Wait();

  bool handle_null = EvalJs(rfh_c_in_b, "handle === null;").ExtractBool();

  tab_b->Close();
  EXPECT_EQ(GetParam(), handle_null);
}

INSTANTIATE_TEST_SUITE_P(All,
                         PartitionedBlobUrlUsagePolicyBrowserTestP,
                         testing::Values(true, false));

}  // namespace policy
