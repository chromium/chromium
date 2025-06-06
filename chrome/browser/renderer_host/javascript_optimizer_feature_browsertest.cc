// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_host_resolver.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

class JavascriptOptimizerBrowserTest : public PlatformBrowserTest {
 public:
  void SetUpOnMainThread() override {
    PlatformBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    content::SetupCrossSiteRedirector(&embedded_https_test_server());
  }

  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  bool AreV8OptimizationsDisabledOnActiveWebContents() {
    return web_contents()
        ->GetPrimaryMainFrame()
        ->GetProcess()
        ->AreV8OptimizationsDisabled();
  }
};

class JavascriptOptimizerBrowserTest_NoOriginKeyedProcessesByDefault
    : public JavascriptOptimizerBrowserTest {
 public:
  JavascriptOptimizerBrowserTest_NoOriginKeyedProcessesByDefault() {
    feature_list_.InitAndDisableFeature(
        features::kOriginKeyedProcessesByDefault);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class JavascriptOptimizerBrowserTest_OriginKeyedProcessesByDefault
    : public JavascriptOptimizerBrowserTest {
 public:
  JavascriptOptimizerBrowserTest_OriginKeyedProcessesByDefault() {
    feature_list_.InitWithFeatures(
        {features::kOriginKeyedProcessesByDefault, features::kSitePerProcess},
        {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Test that V8 optimization is disabled when the user disables v8 optimization
// by default via chrome://settings.
IN_PROC_BROWSER_TEST_F(JavascriptOptimizerBrowserTest,
                       V8SiteSettingDefaultOff) {
  ASSERT_TRUE(embedded_https_test_server().Start());

  auto* map = HostContentSettingsMapFactory::GetForProfile(
      chrome_test_utils::GetProfile(this));
  map->SetDefaultContentSetting(ContentSettingsType::JAVASCRIPT_OPTIMIZER,
                                ContentSetting::CONTENT_SETTING_BLOCK);

  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), embedded_https_test_server().GetURL("/simple.html")));
  EXPECT_TRUE(AreV8OptimizationsDisabledOnActiveWebContents());
}

// Test that V8 optimization is disabled when the user disables v8 optimization
// via chrome://settings for a specific site.
IN_PROC_BROWSER_TEST_F(JavascriptOptimizerBrowserTest,
                       DisabledViaSiteSpecificSetting) {
  ASSERT_TRUE(embedded_https_test_server().Start());

  auto* map = HostContentSettingsMapFactory::GetForProfile(
      chrome_test_utils::GetProfile(this));
  map->SetDefaultContentSetting(ContentSettingsType::JAVASCRIPT_OPTIMIZER,
                                ContentSetting::CONTENT_SETTING_ALLOW);
  map->SetContentSettingCustomScope(
      ContentSettingsPattern::FromString("https://a.com:*"),
      ContentSettingsPattern::FromString("*"),
      ContentSettingsType::JAVASCRIPT_OPTIMIZER,
      ContentSetting::CONTENT_SETTING_BLOCK);

  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_https_test_server().GetURL("a.com", "/simple.html")));
  EXPECT_TRUE(AreV8OptimizationsDisabledOnActiveWebContents());
}

// Test when an origin that matches an exception is loaded in a subframe, the
// origin is not isolated and the exception is not applied. This test does not
// apply if OriginKeyedProcessesByDefault is enabled because in that mode all
// origins would already be isolated on first navigation.
IN_PROC_BROWSER_TEST_F(
    JavascriptOptimizerBrowserTest_NoOriginKeyedProcessesByDefault,
    ExceptionOriginLoadedInSubframeIsNotIsolatedOnFirstNavigation) {
  ASSERT_TRUE(embedded_https_test_server().Start());

  auto* policy = content::ChildProcessSecurityPolicy::GetInstance();
  auto* map = HostContentSettingsMapFactory::GetForProfile(
      chrome_test_utils::GetProfile(this));
  map->SetDefaultContentSetting(ContentSettingsType::JAVASCRIPT_OPTIMIZER,
                                ContentSetting::CONTENT_SETTING_ALLOW);
  map->SetContentSettingCustomScope(
      ContentSettingsPattern::FromString("https://b.com:*"),
      ContentSettingsPattern::FromString("*"),
      ContentSettingsType::JAVASCRIPT_OPTIMIZER,
      ContentSetting::CONTENT_SETTING_BLOCK);

  // Request a.com, which loads b.com in an iframe.
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_https_test_server().GetURL("a.com", "/iframe_cross_site.html")));
  content::RenderFrameHost* child_frame =
      content::ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);
  // b.com will be marked for isolation when loaded in a subframe. But the
  // exception won't be followed until it is loaded in a future browsing
  // context.
  EXPECT_TRUE(child_frame->GetLastCommittedOrigin().DomainIs("b.com"));
  EXPECT_TRUE(policy->IsIsolatedSiteFromSource(
      url::Origin::Create(GURL("https://b.com/")),
      content::ChildProcessSecurityPolicy::IsolatedOriginSource::
          USER_TRIGGERED));

  if (content::SiteIsolationPolicy::UseDedicatedProcessesForAllSites()) {
    // If dedicated processes are used for all sites, then b.com's rules will be
    // followed so this case doesn't apply. So in these cases, we are verifying
    // that new exceptions are respected in sub-frames.
    EXPECT_TRUE(child_frame->GetProcess()->AreV8OptimizationsDisabled());
    EXPECT_TRUE(child_frame->GetSiteInstance()->RequiresDedicatedProcess());
  } else {
    EXPECT_FALSE(child_frame->GetProcess()->AreV8OptimizationsDisabled());
    EXPECT_FALSE(child_frame->GetSiteInstance()->RequiresDedicatedProcess());
  }

  // Confirm that the exception applies when b.com is loaded in a new
  // BrowsingInstance. (This is because NavigateToURL performs a
  // browser-initiated navigation which will swap BrowsingInstances when
  // navigating cross-site.)
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_https_test_server().GetURL("b.com", "/title1.html")));
  EXPECT_TRUE(AreV8OptimizationsDisabledOnActiveWebContents());
  EXPECT_TRUE(web_contents()
                  ->GetPrimaryMainFrame()
                  ->GetSiteInstance()
                  ->RequiresDedicatedProcess());

  EXPECT_TRUE(web_contents()
                  ->GetPrimaryMainFrame()
                  ->GetProcess()
                  ->IsProcessLockedToSiteForTesting());
}

// Test that when an origin that matches an exception is loaded in a main frame
// first, then if the origin is loaded in a subframe later, the origin will be
// isolated and the exception will be applied in both cases.
IN_PROC_BROWSER_TEST_F(
    JavascriptOptimizerBrowserTest_NoOriginKeyedProcessesByDefault,
    ExceptionOriginLoadedFirstWillBeIsolatedInSubframe) {
  ASSERT_TRUE(embedded_https_test_server().Start());

  auto* policy = content::ChildProcessSecurityPolicy::GetInstance();
  auto* map = HostContentSettingsMapFactory::GetForProfile(
      chrome_test_utils::GetProfile(this));
  map->SetDefaultContentSetting(ContentSettingsType::JAVASCRIPT_OPTIMIZER,
                                ContentSetting::CONTENT_SETTING_ALLOW);
  map->SetContentSettingCustomScope(
      ContentSettingsPattern::FromString("https://b.com:*"),
      ContentSettingsPattern::FromString("*"),
      ContentSettingsType::JAVASCRIPT_OPTIMIZER,
      ContentSetting::CONTENT_SETTING_BLOCK);

  // Navigate to b.com directly.
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_https_test_server().GetURL("b.com", "/title1.html")));
  EXPECT_TRUE(AreV8OptimizationsDisabledOnActiveWebContents());
  EXPECT_TRUE(web_contents()
                  ->GetPrimaryMainFrame()
                  ->GetSiteInstance()
                  ->RequiresDedicatedProcess());
  EXPECT_TRUE(web_contents()
                  ->GetPrimaryMainFrame()
                  ->GetProcess()
                  ->IsProcessLockedToSiteForTesting());

  // Then navigate to a.com that embeds b.com in an iframe.
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_https_test_server().GetURL("a.com", "/iframe_cross_site.html")));
  content::RenderFrameHost* child_frame =
      content::ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);
  // Since b.com is already marked for isolation, when loaded as a subframe, the
  // subframe will still have the isolation (and the js-opt setting) applied.
  EXPECT_FALSE(AreV8OptimizationsDisabledOnActiveWebContents());
  EXPECT_TRUE(child_frame->GetLastCommittedOrigin().DomainIs("b.com"));
  EXPECT_TRUE(policy->IsIsolatedSiteFromSource(
      url::Origin::Create(GURL("https://b.com/")),
      content::ChildProcessSecurityPolicy::IsolatedOriginSource::
          USER_TRIGGERED));
  EXPECT_TRUE(child_frame->GetProcess()->AreV8OptimizationsDisabled());
  EXPECT_TRUE(child_frame->GetSiteInstance()->RequiresDedicatedProcess());
  EXPECT_TRUE(child_frame->GetProcess()->IsProcessLockedToSiteForTesting());
}

// Test that when a rule is removed during a session, then the origin will still
// be isolated but the updated js-opt setting will be applied. This test does
// not apply under OriginKeyedProcessesByDefault because all origins would be
// isolated.
IN_PROC_BROWSER_TEST_F(
    JavascriptOptimizerBrowserTest_NoOriginKeyedProcessesByDefault,
    RemoveRuleOriginIsStillIsolatedButIsAllowed) {
  ASSERT_TRUE(embedded_https_test_server().Start());

  auto* policy = content::ChildProcessSecurityPolicy::GetInstance();
  auto* map = HostContentSettingsMapFactory::GetForProfile(
      chrome_test_utils::GetProfile(this));
  map->SetDefaultContentSetting(ContentSettingsType::JAVASCRIPT_OPTIMIZER,
                                ContentSetting::CONTENT_SETTING_ALLOW);
  map->SetContentSettingCustomScope(
      ContentSettingsPattern::FromString("https://a.com:*"),
      ContentSettingsPattern::FromString("*"),
      ContentSettingsType::JAVASCRIPT_OPTIMIZER,
      ContentSetting::CONTENT_SETTING_BLOCK);

  // Before any navigation, a site with an exception is not isolated.
  EXPECT_FALSE(policy->IsIsolatedSiteFromSource(
      url::Origin::Create(GURL("https://a.com/")),
      content::ChildProcessSecurityPolicy::IsolatedOriginSource::
          USER_TRIGGERED));

  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_https_test_server().GetURL("a.com", "/simple.html")));

  // After navigation, the site will be isolated.
  EXPECT_TRUE(policy->IsIsolatedSiteFromSource(
      url::Origin::Create(GURL("https://a.com/")),
      content::ChildProcessSecurityPolicy::IsolatedOriginSource::
          USER_TRIGGERED));

  EXPECT_TRUE(AreV8OptimizationsDisabledOnActiveWebContents());
  EXPECT_TRUE(web_contents()->GetSiteInstance()->RequiresDedicatedProcess());
  EXPECT_TRUE(web_contents()
                  ->GetPrimaryMainFrame()
                  ->GetProcess()
                  ->IsProcessLockedToSiteForTesting());

  // Delete the custom setting
  map->SetContentSettingCustomScope(
      ContentSettingsPattern::FromString("https://a.com:*"),
      ContentSettingsPattern::FromString("*"),
      ContentSettingsType::JAVASCRIPT_OPTIMIZER,
      ContentSetting::CONTENT_SETTING_DEFAULT);

  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_https_test_server().GetURL("a.com", "/simple.html")));

  // Despite the settings change, the new a.com document should still be
  // isolated because policy changes that result in no longer isolating an
  // origin only take effect after restart.
  EXPECT_TRUE(policy->IsIsolatedSiteFromSource(
      url::Origin::Create(GURL("https://a.com/")),
      content::ChildProcessSecurityPolicy::IsolatedOriginSource::
          USER_TRIGGERED));
  EXPECT_TRUE(web_contents()->GetSiteInstance()->RequiresDedicatedProcess());
  EXPECT_TRUE(web_contents()
                  ->GetPrimaryMainFrame()
                  ->GetProcess()
                  ->IsProcessLockedToSiteForTesting());
  // Additionally, since a.com no longer has a specific policy, the loaded
  // document should follow the default setting (allow optimizations).
  EXPECT_FALSE(AreV8OptimizationsDisabledOnActiveWebContents());
}

// Test that when an exception exists for a.com, navigation to sub.a.com will
// also have the setting applied.
IN_PROC_BROWSER_TEST_F(
    JavascriptOptimizerBrowserTest_NoOriginKeyedProcessesByDefault,
    ExceptionForSiteAppliesToSubSite) {
  ASSERT_TRUE(embedded_https_test_server().Start());
  auto* map = HostContentSettingsMapFactory::GetForProfile(
      chrome_test_utils::GetProfile(this));
  map->SetDefaultContentSetting(ContentSettingsType::JAVASCRIPT_OPTIMIZER,
                                ContentSetting::CONTENT_SETTING_ALLOW);
  map->SetContentSettingCustomScope(
      ContentSettingsPattern::FromString("https://a.com:*"),
      ContentSettingsPattern::FromString("*"),
      ContentSettingsType::JAVASCRIPT_OPTIMIZER,
      ContentSetting::CONTENT_SETTING_BLOCK);

  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_https_test_server().GetURL("a.com", "/simple.html")));

  EXPECT_TRUE(AreV8OptimizationsDisabledOnActiveWebContents());
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_https_test_server().GetURL("sub.a.com", "/simple.html")));

  // True under site isolation.
  EXPECT_TRUE(AreV8OptimizationsDisabledOnActiveWebContents());
}

// Under origin isolation, test that when an exception exists for a.com,
// navigation to sub.a.com will not have the setting applied. This is because
// the origin is passed in to AreV8OptimizationsDisabledForSite() when
// evaluating the rule.
IN_PROC_BROWSER_TEST_F(
    JavascriptOptimizerBrowserTest_OriginKeyedProcessesByDefault,
    ExceptionForSiteDoesNotApplyToSubSite) {
  if (!content::SiteIsolationPolicy::
          AreOriginKeyedProcessesEnabledByDefault()) {
    GTEST_SKIP()
        << "skipping: OriginKeyedProcessesEnabledByDefault needs to be true";
  }
  ASSERT_TRUE(embedded_https_test_server().Start());
  auto* map = HostContentSettingsMapFactory::GetForProfile(
      chrome_test_utils::GetProfile(this));
  map->SetDefaultContentSetting(ContentSettingsType::JAVASCRIPT_OPTIMIZER,
                                ContentSetting::CONTENT_SETTING_ALLOW);
  map->SetContentSettingCustomScope(
      ContentSettingsPattern::FromString("https://a.com:*"),
      ContentSettingsPattern::FromString("*"),
      ContentSettingsType::JAVASCRIPT_OPTIMIZER,
      ContentSetting::CONTENT_SETTING_BLOCK);

  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_https_test_server().GetURL("a.com", "/simple.html")));

  EXPECT_TRUE(AreV8OptimizationsDisabledOnActiveWebContents());
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_https_test_server().GetURL("sub.a.com", "/simple.html")));

  // False under origin isolation because the origin won't match the content
  // setting for a.com.
  EXPECT_FALSE(AreV8OptimizationsDisabledOnActiveWebContents());
}

// Test that if there's a rule for a.com that differs from the default, then the
// user can't specify a rule for sub.a.com that contradicts that rule.
// TODO(crbug.com/413695645): Make it possible for users to specify overrides so
// that sub.a.com's behavior can differ from a.com's behavior.
IN_PROC_BROWSER_TEST_F(JavascriptOptimizerBrowserTest,
                       ExceptionForSiteAppliesToSubSiteButCannotBeOverridden) {
  ASSERT_TRUE(embedded_https_test_server().Start());
  auto* policy = content::ChildProcessSecurityPolicy::GetInstance();
  auto* map = HostContentSettingsMapFactory::GetForProfile(
      chrome_test_utils::GetProfile(this));
  map->SetDefaultContentSetting(ContentSettingsType::JAVASCRIPT_OPTIMIZER,
                                ContentSetting::CONTENT_SETTING_ALLOW);
  map->SetContentSettingCustomScope(
      ContentSettingsPattern::FromString("https://sub.a.com:*"),
      ContentSettingsPattern::FromString("*"),
      ContentSettingsType::JAVASCRIPT_OPTIMIZER,
      ContentSetting::CONTENT_SETTING_ALLOW);
  map->SetContentSettingCustomScope(
      ContentSettingsPattern::FromString("https://a.com:*"),
      ContentSettingsPattern::FromString("*"),
      ContentSettingsType::JAVASCRIPT_OPTIMIZER,
      ContentSetting::CONTENT_SETTING_BLOCK);

  // Since this exception matches the default, it will not be isolated.
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_https_test_server().GetURL("sub.a.com", "/simple.html")));

  EXPECT_FALSE(policy->IsIsolatedSiteFromSource(
      url::Origin::Create(GURL("https://a.com/")),
      content::ChildProcessSecurityPolicy::IsolatedOriginSource::
          USER_TRIGGERED));

  if (content::AreStrictSiteInstancesEnabled() &&
      !content::SiteIsolationPolicy::
          AreOriginKeyedProcessesEnabledByDefault()) {
    // if a.com is isolated already (as is the case with full site isolation)
    // or if DefaultSiteInstanceGroups are enabled, and origin isolation is not
    // used, the navigation to sub.a.com will be made in a SiteInstance with a
    // "a.com" site URL, which will match a.com BLOCK rule.
    EXPECT_TRUE(AreV8OptimizationsDisabledOnActiveWebContents());
  } else {
    // If nothing is isolated by default (like on Android), we'll navigate in a
    // default SiteInstance which won't match that rule and will instead
    // retrieve the default rule. TODO(crbug.com/413695645): make it possible
    // for users to specify overrides so that sub.a.com's behavior can differ
    // from a.com's behavior.
    EXPECT_FALSE(AreV8OptimizationsDisabledOnActiveWebContents());
  }

  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_https_test_server().GetURL("a.com", "/simple.html")));

  EXPECT_TRUE(AreV8OptimizationsDisabledOnActiveWebContents());

  // Navigate back to sub.a.com, and we would like for js-opt to be enabled, but
  // they are disabled instead, because url provided to
  // AreV8OptimizationsDisabledForSite is a.com, which matches the block rule.
  // Ideally we'd be able to specify rules here, but to do that we need to pass
  // in the origin instead of the site. Currently, the site is passed because
  // sub.a.com is not origin isolated. TODO(crbug.com/413695645): Make it
  // possible for users to specify overrides so that sub.a.com's behavior can
  // differ from a.com's behavior.
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_https_test_server().GetURL("sub.a.com", "/simple.html")));
  if (content::SiteIsolationPolicy::AreOriginKeyedProcessesEnabledByDefault()) {
    // Under origin isolation, the rule won't match sub.a.com, so optimizers
    // remain enabled.
    EXPECT_FALSE(AreV8OptimizationsDisabledOnActiveWebContents());
  } else {
    // Under site isolation, sub.a.com will be evaluated as a.com so the rule
    // will match.
    EXPECT_TRUE(AreV8OptimizationsDisabledOnActiveWebContents());
  }
}

// Test that a rule can be specified for sub.a.com. and a.com can have different
// behavior.
IN_PROC_BROWSER_TEST_F(
    JavascriptOptimizerBrowserTest,
    RuleForSubSiteCanBeSpecifiedAndSiteCanStillFollowDefaultRule) {
#if BUILDFLAG(IS_LINUX)
  // TODO(421325694): This test fails on linux when bfcache is disabled.
  if (!base::FeatureList::IsEnabled(features::kBackForwardCache)) {
    GTEST_SKIP();
  }
#endif

  ASSERT_TRUE(embedded_https_test_server().Start());
  auto* policy = content::ChildProcessSecurityPolicy::GetInstance();
  auto* map = HostContentSettingsMapFactory::GetForProfile(
      chrome_test_utils::GetProfile(this));
  map->SetDefaultContentSetting(ContentSettingsType::JAVASCRIPT_OPTIMIZER,
                                ContentSetting::CONTENT_SETTING_ALLOW);
  map->SetContentSettingCustomScope(
      ContentSettingsPattern::FromString("https://sub.a.com:*"),
      ContentSettingsPattern::FromString("*"),
      ContentSettingsType::JAVASCRIPT_OPTIMIZER,
      ContentSetting::CONTENT_SETTING_BLOCK);
  map->SetContentSettingCustomScope(
      ContentSettingsPattern::FromString("https://a.com:*"),
      ContentSettingsPattern::FromString("*"),
      ContentSettingsType::JAVASCRIPT_OPTIMIZER,
      ContentSetting::CONTENT_SETTING_ALLOW);

  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_https_test_server().GetURL("a.com", "/simple.html")));

  EXPECT_FALSE(policy->IsIsolatedSiteFromSource(
      url::Origin::Create(GURL("https://a.com/")),
      content::ChildProcessSecurityPolicy::IsolatedOriginSource::
          USER_TRIGGERED));

  EXPECT_FALSE(AreV8OptimizationsDisabledOnActiveWebContents());

  // In this case, since a.com's policy matched the default, a.com is not
  // isolated, but sub.a.com will be isolated so sub.a.com follows its
  // exception.
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_https_test_server().GetURL("sub.a.com", "/simple.html")));

  EXPECT_TRUE(AreV8OptimizationsDisabledOnActiveWebContents());

  // If we now navigate back to a.com, just like before, the a.com will still
  // not be isolated, and optimizers will be allowed.
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_https_test_server().GetURL("a.com", "/simple.html")));
  EXPECT_FALSE(policy->IsIsolatedSiteFromSource(
      url::Origin::Create(GURL("https://a.com/")),
      content::ChildProcessSecurityPolicy::IsolatedOriginSource::
          USER_TRIGGERED));
  EXPECT_FALSE(AreV8OptimizationsDisabledOnActiveWebContents());
}

// Test that exceptions which match the main frame are not propagated down to
// subframes from different sites. Additionally, if the subframe is later
// navigated to a site that matches the main frame, the main frame's exception
// will apply.
IN_PROC_BROWSER_TEST_F(
    JavascriptOptimizerBrowserTest_NoOriginKeyedProcessesByDefault,
    ExceptionForTopFrameDoesNotApplyToSubFrame) {
  ASSERT_TRUE(embedded_https_test_server().Start());
  auto* map = HostContentSettingsMapFactory::GetForProfile(
      chrome_test_utils::GetProfile(this));
  map->SetDefaultContentSetting(ContentSettingsType::JAVASCRIPT_OPTIMIZER,
                                ContentSetting::CONTENT_SETTING_ALLOW);
  map->SetContentSettingCustomScope(
      ContentSettingsPattern::FromString("https://a.com:*"),
      ContentSettingsPattern::FromString("*"),
      ContentSettingsType::JAVASCRIPT_OPTIMIZER,
      ContentSetting::CONTENT_SETTING_BLOCK);

  // Navigate to a.com which embeds b.com.
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_https_test_server().GetURL("a.com", "/iframe_cross_site.html")));

  // The top-level frame should have optimizations disabled.
  EXPECT_TRUE(AreV8OptimizationsDisabledOnActiveWebContents());

  // But the frame for b.com follows the default policy.
  content::RenderFrameHost* child_frame =
      content::ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);
  EXPECT_FALSE(child_frame->GetProcess()->AreV8OptimizationsDisabled());
  EXPECT_TRUE(child_frame->GetLastCommittedOrigin().DomainIs("b.com"));

  // Now, navigate the child_frame to sub.a.com and confirm that a.com's disable
  // setting applies to sub.a.com.
  ASSERT_TRUE(content::NavigateIframeToURL(
      web_contents(), "frame1",
      embedded_https_test_server().GetURL("sub.a.com", "/simple.html")));

  child_frame = content::ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);
  EXPECT_EQ(child_frame->GetLastCommittedURL().host(), "sub.a.com");
  // True under site isolation but not origin isolation.
  EXPECT_TRUE(child_frame->GetProcess()->AreV8OptimizationsDisabled());
}

// Test that sites isolated due to JavaScript optimization setting will not be
// put into processes for other sites when over the limit. This should already
// be covered by other IsolatedOriginTests, but this case ensures that
// JavaScript optimization is handled correctly.
IN_PROC_BROWSER_TEST_F(JavascriptOptimizerBrowserTest, ProcessLimitWorks) {
  content::RenderProcessHost::SetMaxRendererProcessCount(1);
  ASSERT_TRUE(embedded_https_test_server().Start());

  auto* map = HostContentSettingsMapFactory::GetForProfile(
      chrome_test_utils::GetProfile(this));
  map->SetDefaultContentSetting(ContentSettingsType::JAVASCRIPT_OPTIMIZER,
                                ContentSetting::CONTENT_SETTING_ALLOW);
  map->SetContentSettingCustomScope(
      ContentSettingsPattern::FromString("https://b.com:*"),
      ContentSettingsPattern::FromString("*"),
      ContentSettingsType::JAVASCRIPT_OPTIMIZER,
      ContentSetting::CONTENT_SETTING_BLOCK);

  // Navigate to b.com first to ensure it is on the isolated origins list.
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_https_test_server().GetURL("b.com", "/simple.html")));

  // Visit a.com in a new BrowsingInstance, which iframes b.com and c.com.
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_https_test_server().GetURL("a.com", "/iframe_cross_site.html")));

  content::RenderFrameHost* a_com_frame = web_contents()->GetPrimaryMainFrame();
  content::RenderFrameHost* b_com_frame = content::ChildFrameAt(a_com_frame, 0);
  content::RenderFrameHost* c_com_frame = content::ChildFrameAt(a_com_frame, 1);

  if (content::SiteIsolationPolicy::UseDedicatedProcessesForAllSites()) {
    // When all sites are isolated, each frame should be in its own process.
    EXPECT_NE(a_com_frame->GetProcess(), b_com_frame->GetProcess());
    EXPECT_NE(c_com_frame->GetProcess(), b_com_frame->GetProcess());
    EXPECT_NE(a_com_frame->GetProcess(), c_com_frame->GetProcess());

  } else {
    // When partial site isolation is enabled, the result is that b.com should
    // be put into its own process despite Chrome being at the soft process
    // limit. a.com and c.com will end up together.
    EXPECT_NE(a_com_frame->GetProcess(), b_com_frame->GetProcess());
    EXPECT_NE(c_com_frame->GetProcess(), b_com_frame->GetProcess());
    EXPECT_EQ(a_com_frame->GetProcess(), c_com_frame->GetProcess());
  }
}
