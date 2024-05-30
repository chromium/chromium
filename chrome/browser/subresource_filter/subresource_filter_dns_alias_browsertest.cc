// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>

#include "base/strings/pattern.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_database_helper.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/browser/subresource_filter/subresource_filter_browser_test_harness.h"
#include "chrome/browser/ui/browser.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/safe_browsing/core/browser/db/v4_test_util.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_throttle_manager.h"
#include "components/subresource_filter/content/browser/test_ruleset_publisher.h"
#include "components/subresource_filter/content/shared/browser/ruleset_service.h"
#include "components/subresource_filter/core/browser/async_document_subresource_filter.h"
#include "components/subresource_filter/core/browser/async_document_subresource_filter_test_utils.h"
#include "components/subresource_filter/core/browser/subresource_filter_constants.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/subresource_filter/core/browser/subresource_filter_features_test_support.h"
#include "components/subresource_filter/core/common/activation_decision.h"
#include "components/subresource_filter/core/common/common_features.h"
#include "components/subresource_filter/core/common/test_ruleset_creator.h"
#include "components/subresource_filter/core/common/test_ruleset_utils.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "components/url_pattern_index/proto/rules.pb.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace subresource_filter {

using ActivationLevel = subresource_filter::mojom::ActivationLevel;

// Struct allows for definition of PrintToString method below.
struct Level {
  ActivationLevel level;
};

// Used by ::testing::PrintToStringParamName().
std::string PrintToString(Level level) {
  switch (level.level) {
    case ActivationLevel::kEnabled:
      return "ActivationEnabled";
    case ActivationLevel::kDryRun:
      return "ActivationDryRun";
    case ActivationLevel::kDisabled:
      return "ActivationDisabled";
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

namespace {

void AddHostResolverRules(net::RuleBasedHostResolverProc* host_resolver) {
  host_resolver->AddIPLiteralRuleWithDnsAliases(
      "a.com", "127.0.0.1", {"alias1.com", "example.com", "example.org"});
  host_resolver->AddIPLiteralRuleWithDnsAliases(
      "cname-to-bad.com", "127.0.0.1",
      {"alias.com", "example.com", "disallowed.com", "bad.com",
       "cname-to-bad.com"});
}

std::vector<Level> GetActivationLevels() {
  static_assert(ActivationLevel::kMaxValue == ActivationLevel::kEnabled,
                "Update these tests if more activation levels are added");
  return {{ActivationLevel::kEnabled},
          {ActivationLevel::kDryRun},
          {ActivationLevel::kDisabled}};
}

}  // namespace

class SubresourceFilterDnsAliasResourceLoaderBrowserTest
    : public SubresourceFilterBrowserTest,
      public ::testing::WithParamInterface<Level> {
 public:
  SubresourceFilterDnsAliasResourceLoaderBrowserTest() {
    feature_list_.InitWithFeatures(
        {kAdTagging,
         blink::features::kSendCnameAliasesToSubresourceFilterFromRenderer},
        {} /* disabled_features */);
  }

  ~SubresourceFilterDnsAliasResourceLoaderBrowserTest() override = default;

 protected:
  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    AddHostResolverRules(host_resolver());
    SubresourceFilterBrowserTest::SetUpOnMainThread();
  }

  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(Renderer,
                         SubresourceFilterDnsAliasResourceLoaderBrowserTest,
                         ::testing::ValuesIn(GetActivationLevels()),
                         ::testing::PrintToStringParamName());

IN_PROC_BROWSER_TEST_P(SubresourceFilterDnsAliasResourceLoaderBrowserTest,
                       CheckDnsAliasesFromRenderer) {
  ActivationLevel level = GetParam().level;
  Configuration config(
      level, subresource_filter::ActivationScope::ACTIVATION_LIST,
      subresource_filter::ActivationList::PHISHING_INTERSTITIAL);
  ResetConfiguration(std::move(config));

  GURL url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(cname-to-bad)"));
  ConfigureAsPhishingURL(url);
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithSubstrings({"disallowed"}));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents != nullptr);

  EXPECT_TRUE(NavigateToURL(web_contents, url));

  content::RenderFrameHost* child_rfh =
      ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0u);

  if (level == ActivationLevel::kEnabled)
    EXPECT_FALSE(WasParsedScriptElementLoaded(child_rfh));
  else
    EXPECT_TRUE(WasParsedScriptElementLoaded(child_rfh));
}

class SubresourceFilterDnsAliasFilteringThrottleBrowserTest
    : public SubresourceFilterBrowserTest,
      public ::testing::WithParamInterface<Level> {
 public:
  SubresourceFilterDnsAliasFilteringThrottleBrowserTest() {
    feature_list_.InitWithFeatures(
        {kAdTagging, features::kSendCnameAliasesToSubresourceFilterFromBrowser},
        {} /* disabled_features */);
  }

  ~SubresourceFilterDnsAliasFilteringThrottleBrowserTest() override = default;

 protected:
  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    AddHostResolverRules(host_resolver());
    SubresourceFilterBrowserTest::SetUpOnMainThread();
  }

  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(Browser,
                         SubresourceFilterDnsAliasFilteringThrottleBrowserTest,
                         ::testing::ValuesIn(GetActivationLevels()),
                         ::testing::PrintToStringParamName());

IN_PROC_BROWSER_TEST_P(SubresourceFilterDnsAliasFilteringThrottleBrowserTest,
                       CheckDnsAliasesFromBrowser) {
  ActivationLevel level = GetParam().level;
  Configuration config(
      level, subresource_filter::ActivationScope::ACTIVATION_LIST,
      subresource_filter::ActivationList::PHISHING_INTERSTITIAL);
  ResetConfiguration(std::move(config));

  GURL url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(cname-to-bad)"));
  ConfigureAsPhishingURL(url);
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithSubstrings({"disallowed"}));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents != nullptr);

  EXPECT_TRUE(NavigateToURL(web_contents, url));

  content::RenderFrameHost* main_rfh = web_contents->GetPrimaryMainFrame();
  content::RenderFrameHost* child_rfh =
      ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0u);

  if (level == ActivationLevel::kEnabled) {
    EXPECT_EQ(GURL(), child_rfh->GetLastCommittedURL());
  } else {
    GURL main_url = main_rfh->GetLastCommittedURL();
    GURL expected_child_url(
        base::StrCat({"http://cname-to-bad.com:", main_url.port(),
                      "/cross_site_iframe_factory.html?cname-to-bad()"}));

    EXPECT_EQ(expected_child_url, child_rfh->GetLastCommittedURL());
  }
}

}  // namespace subresource_filter
