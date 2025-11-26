// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/content_settings/generated_javascript_optimizer_pref.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/browser/site_protection/site_familiarity_fetcher.h"
#include "chrome/browser/site_protection/site_familiarity_utils.h"

#if !BUILDFLAG(IS_ANDROID)
#include "base/test/bind.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "chrome/browser/ui/views/page_action/action_ids.h"
#include "chrome/browser/ui/views/page_action/test_support/page_action_interactive_test_mixin.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#endif  // !BUILDFLAG(IS_ANDROID)

#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/features.h"
#include "components/history/core/browser/history_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/browser/db/fake_database_manager.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/process_selection_deferring_condition.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_host_resolver.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

typedef site_protection::SiteFamiliarityFetcher::Verdict FamiliarityVerdict;

class JavascriptOptimizerBrowserTest : public PlatformBrowserTest {
 public:
  void SetUpOnMainThread() override {
    PlatformBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    content::SetupCrossSiteRedirector(&embedded_https_test_server());

    embedded_https_test_server().SetCertHostnames(
        {"a.com", "*.a.com", "b.com", "*.b.com", "unrelated.com"});
    ASSERT_TRUE(embedded_https_test_server().Start());
  }

  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  Profile* profile() { return chrome_test_utils::GetProfile(this); }

  bool AreV8OptimizationsDisabledForRenderFrame(content::RenderFrameHost* rfh) {
    return rfh->GetProcess()->AreV8OptimizationsDisabled();
  }

  bool AreV8OptimizationsDisabledOnActiveWebContents() {
    return AreV8OptimizationsDisabledForRenderFrame(
        web_contents()->GetPrimaryMainFrame());
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
  auto* map = HostContentSettingsMapFactory::GetForProfile(profile());
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
  auto* map = HostContentSettingsMapFactory::GetForProfile(profile());
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
  auto* policy = content::ChildProcessSecurityPolicy::GetInstance();
  auto* map = HostContentSettingsMapFactory::GetForProfile(profile());
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
  auto* policy = content::ChildProcessSecurityPolicy::GetInstance();
  auto* map = HostContentSettingsMapFactory::GetForProfile(profile());
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
  auto* policy = content::ChildProcessSecurityPolicy::GetInstance();
  auto* map = HostContentSettingsMapFactory::GetForProfile(profile());
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

  // Navigate to different origin so that the subsequent navigation to a.com
  // occurs in a different BrowsingInstanceId.
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_https_test_server().GetURL("unrelated.com", "/simple.html")));

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
  auto* map = HostContentSettingsMapFactory::GetForProfile(profile());
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
  auto* map = HostContentSettingsMapFactory::GetForProfile(profile());
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
  auto* policy = content::ChildProcessSecurityPolicy::GetInstance();
  auto* map = HostContentSettingsMapFactory::GetForProfile(profile());
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

  auto* policy = content::ChildProcessSecurityPolicy::GetInstance();
  auto* map = HostContentSettingsMapFactory::GetForProfile(profile());
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
  auto* map = HostContentSettingsMapFactory::GetForProfile(profile());
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
  EXPECT_EQ(child_frame->GetLastCommittedURL().GetHost(), "sub.a.com");
  // True under site isolation but not origin isolation.
  EXPECT_TRUE(child_frame->GetProcess()->AreV8OptimizationsDisabled());
}

// Test that sites isolated due to JavaScript optimization setting will not be
// put into processes for other sites when over the limit. This should already
// be covered by other IsolatedOriginTests, but this case ensures that
// JavaScript optimization is handled correctly.
IN_PROC_BROWSER_TEST_F(JavascriptOptimizerBrowserTest, ProcessLimitWorks) {
  content::RenderProcessHost::SetMaxRendererProcessCount(1);

  auto* map = HostContentSettingsMapFactory::GetForProfile(profile());
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

namespace {

void NavigateChangeV8OptPriorToWindowOpen(content::WebContents* web_contents,
                                          const GURL& navigate_url,
                                          const GURL& window_open_url) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  auto* map = HostContentSettingsMapFactory::GetForProfile(profile);
  map->SetDefaultContentSetting(ContentSettingsType::JAVASCRIPT_OPTIMIZER,
                                ContentSetting::CONTENT_SETTING_ALLOW);
  ASSERT_TRUE(content::NavigateToURL(web_contents, navigate_url));

  // Simulate changing the default v8-optimization preference via
  // chrome://settings in a different tab.
  map->SetDefaultContentSetting(ContentSettingsType::JAVASCRIPT_OPTIMIZER,
                                ContentSetting::CONTENT_SETTING_BLOCK);

  content::TestNavigationObserver popup_observer(nullptr);
  popup_observer.StartWatchingNewWebContents();
  content::EvalJsResult result =
      content::EvalJs(web_contents->GetPrimaryMainFrame(),
                      "window.open(\"" + window_open_url.spec() + "\");");
  popup_observer.Wait();
}

}  // anonymous namespace

// Test that a same-origin window.open() call uses the same process regardless
// of whether the user changed the v8-optimization state.
IN_PROC_BROWSER_TEST_F(
    JavascriptOptimizerBrowserTest,
    ChangeJavascriptOptimizerStatePriorToSameOriginWindowOpen) {
  GURL url = embedded_https_test_server().GetURL("a.com", "/simple.html");
  NavigateChangeV8OptPriorToWindowOpen(web_contents(), url, url);

  std::vector<content::WebContents*> all_web_contents =
      content::GetAllWebContents();
  ASSERT_EQ(2u, all_web_contents.size());
  content::RenderFrameHost* frame0 = all_web_contents[0]->GetPrimaryMainFrame();
  content::RenderFrameHost* frame1 = all_web_contents[1]->GetPrimaryMainFrame();
  EXPECT_EQ(frame0->GetProcess(), frame1->GetProcess());
  EXPECT_EQ(frame0->GetSiteInstance(), frame1->GetSiteInstance());
}

// Test that when the features::kOriginKeyedProcessesByDefault feature is
// disabled that a same-site window.open() call uses the same process regardless
// of whether the user changed the v8-optimization state.
IN_PROC_BROWSER_TEST_F(
    JavascriptOptimizerBrowserTest_NoOriginKeyedProcessesByDefault,
    ChangeJavascriptOptimizerStatePriorToSameSiteWindowOpen) {
  GURL url = embedded_https_test_server().GetURL("a.com", "/simple.html");
  GURL same_site_url =
      embedded_https_test_server().GetURL("foo.a.com", "/simple.html");
  NavigateChangeV8OptPriorToWindowOpen(web_contents(), url, same_site_url);

  std::vector<content::WebContents*> all_web_contents =
      content::GetAllWebContents();
  ASSERT_EQ(2u, all_web_contents.size());
  content::RenderFrameHost* frame0 = all_web_contents[0]->GetPrimaryMainFrame();
  content::RenderFrameHost* frame1 = all_web_contents[1]->GetPrimaryMainFrame();
  EXPECT_EQ(frame0->GetProcess(), frame1->GetProcess());
  EXPECT_EQ(frame0->GetSiteInstance(), frame1->GetSiteInstance());
}

namespace {

// content::ProcessSelectionDeferringCondition subclass which sets
// `did_select_final_process` bool passed to constructor when
// ProcessSelectionDeferringCondition::OnWillSelectFinalProcess() is called.
class ProcessSelectionObserverCondition
    : public content::ProcessSelectionDeferringCondition {
 public:
  ProcessSelectionObserverCondition(
      content::NavigationHandle& navigation_handle,
      bool* did_select_final_process)
      : content::ProcessSelectionDeferringCondition(navigation_handle),
        did_select_final_process_(did_select_final_process) {}
  ~ProcessSelectionObserverCondition() override = default;

  content::ProcessSelectionDeferringCondition::Result OnWillSelectFinalProcess(
      base::OnceClosure resume) override {
    *did_select_final_process_ = true;
    return ProcessSelectionDeferringCondition::Result::kProceed;
  }

 private:
  raw_ptr<bool> did_select_final_process_;
};

// ChromeContentBrowserClient subclass which uses DeferringCondition.
class DeferProcessSelectionBrowserClient : public ChromeContentBrowserClient {
 public:
  DeferProcessSelectionBrowserClient() = default;
  ~DeferProcessSelectionBrowserClient() override = default;

  std::vector<std::unique_ptr<content::ProcessSelectionDeferringCondition>>
  CreateProcessSelectionDeferringConditionsForNavigation(
      content::NavigationHandle& navigation_handle) override {
    auto condition = std::make_unique<ProcessSelectionObserverCondition>(
        navigation_handle, &did_select_final_process_);
    std::vector<std::unique_ptr<content::ProcessSelectionDeferringCondition>>
        conditions;
    conditions.push_back(std::move(condition));
    return conditions;
  }

  bool AreV8OptimizationsEnabledForSite(
      content::BrowserContext* browser_context,
      const std::optional<base::SafeRef<content::ProcessSelectionUserData>>&
          process_selection_user_data,
      const GURL& site_url) override {
    return !should_disable_v8_optimizations_;
  }

  bool DidSelectFinalProcess() { return did_select_final_process_; }

  bool should_disable_v8_optimizations_ = false;

 private:
  bool did_select_final_process_ = false;
};

}  // anonymous namespace

class JavascriptOptimizerBrowserTest_CustomDeferralCondition
    : public JavascriptOptimizerBrowserTest {
 public:
  JavascriptOptimizerBrowserTest_CustomDeferralCondition() {
    feature_list_
        .InitWithFeatures(/*enabled_features=*/
                          {features::kProcessSelectionDeferringConditions,
                           content_settings::features::
                               kBlockV8OptimizerOnUnfamiliarSitesSetting},
                          /*disabled_features=*/{});
  }

  void SetUpOnMainThread() override {
    browser_client_ = std::make_unique<DeferProcessSelectionBrowserClient>();
    old_browser_client_ =
        content::SetBrowserClientForTesting(browser_client_.get());
    JavascriptOptimizerBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    JavascriptOptimizerBrowserTest::TearDownOnMainThread();
    content::SetBrowserClientForTesting(old_browser_client_.get());
    browser_client_.reset();
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<DeferProcessSelectionBrowserClient> browser_client_;
  raw_ptr<content::ContentBrowserClient> old_browser_client_;
};

// Test that crbug.com/441727826 is fixed. Test that navigations use the
// v8-optimizer state at final process selection time and not prior.
IN_PROC_BROWSER_TEST_F(JavascriptOptimizerBrowserTest_CustomDeferralCondition,
                       ChangeJavascriptOptimizerStatePriorToProcessSelection) {
  const GURL kTestUrl =
      embedded_https_test_server().GetURL("a.com", "/simple.html");

  browser_client_->should_disable_v8_optimizations_ = false;

  content::TestNavigationObserver navigation_observer(web_contents(), 1);

  web_contents()->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(kTestUrl));
  EXPECT_FALSE(browser_client_->DidSelectFinalProcess());

  browser_client_->should_disable_v8_optimizations_ = true;

  navigation_observer.Wait();
  EXPECT_TRUE(browser_client_->DidSelectFinalProcess());
  content::RenderFrameHost* frame = web_contents()->GetPrimaryMainFrame();
  EXPECT_TRUE(frame->GetProcess()->AreV8OptimizationsDisabled());
}

// Base class for integration tests which enable/disable the "disable JavaScript
// optimization for unfamiliar sites" feature.
class JavascriptOptimizerBrowserTest_UseSiteFamiliarityBase
    : public JavascriptOptimizerBrowserTest {
 public:
  JavascriptOptimizerBrowserTest_UseSiteFamiliarityBase() = default;
  ~JavascriptOptimizerBrowserTest_UseSiteFamiliarityBase() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    JavascriptOptimizerBrowserTest::SetUpCommandLine(command_line);

    if (ShouldForceSitePerProcess()) {
      command_line->AppendSwitch(::switches::kSitePerProcess);
    }

    if (ShouldEnableSiteFamiliarityFeature()) {
      feature_list_
          .InitWithFeatures(/*enabled_features=*/
                            {features::kProcessSelectionDeferringConditions,
                             content_settings::features::
                                 kBlockV8OptimizerOnUnfamiliarSitesSetting},
                            /*disabled_features=*/{});
    } else {
      feature_list_.InitWithFeatures(
          /*enabled_features=*/{},
          /*disabled_features=*/
          {features::kProcessSelectionDeferringConditions,
           content_settings::features::
               kBlockV8OptimizerOnUnfamiliarSitesSetting});
    }
  }

  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    JavascriptOptimizerBrowserTest::CreatedBrowserMainParts(browser_main_parts);
    // Test UI manager and test database manager should be set before
    // the browser is started but after threads are created.
    factory_.SetTestDatabaseManager(
        new safe_browsing::FakeSafeBrowsingDatabaseManager(
            content::GetUIThreadTaskRunner({})));
    safe_browsing::SafeBrowsingService::RegisterFactory(&factory_);
  }

  void SetUpOnMainThread() override {
    JavascriptOptimizerBrowserTest::SetUpOnMainThread();

    auto* map = HostContentSettingsMapFactory::GetForProfile(profile());
    map->SetDefaultContentSetting(ContentSettingsType::JAVASCRIPT_OPTIMIZER,
                                  ContentSetting::CONTENT_SETTING_ALLOW);
    profile()->GetPrefs()->SetBoolean(
        prefs::kJavascriptOptimizerBlockedForUnfamiliarSites, true);
    EXPECT_EQ(ShouldEnableSiteFamiliarityFeature(),
              site_protection::AreV8OptimizationsDisabledOnUnfamiliarSites(
                  profile()));
  }

  void RunCallbackAndStoreVerdict(const base::RepeatingClosure& callback,
                                  FamiliarityVerdict* verdict_out,
                                  FamiliarityVerdict verdict) {
    *verdict_out = verdict;
    callback.Run();
  }

  void CheckSiteFamiliarity(const GURL& url,
                            FamiliarityVerdict expected_verdict) {
    site_protection::SiteFamiliarityFetcher fetcher(profile());
    FamiliarityVerdict verdict = FamiliarityVerdict::kFamiliar;
    FamiliarityVerdict* verdict_ptr = &verdict;
    base::RunLoop run_loop;
    fetcher.Start(
        url, base::BindOnce(
                 &JavascriptOptimizerBrowserTest_UseSiteFamiliarityBase::
                     RunCallbackAndStoreVerdict,
                 base::Unretained(this), run_loop.QuitClosure(), verdict_ptr));
    run_loop.Run();
    EXPECT_EQ(expected_verdict, verdict);
  }

  void CheckUnfamiliarSite(bool expect_v8_optimizations_enabled) {
    const GURL kTestUrl =
        embedded_https_test_server().GetURL("a.com", "/simple.html");
    ASSERT_TRUE(content::NavigateToURL(web_contents(), kTestUrl));
    EXPECT_EQ(!expect_v8_optimizations_enabled,
              AreV8OptimizationsDisabledOnActiveWebContents());
    CheckSiteFamiliarity(kTestUrl, FamiliarityVerdict::kUnfamiliar);
  }

  void MarkAsFamiliar(const GURL& url) {
    // Mark site as familiar by adding a chrome://history entry older than 24
    // hours.
    HistoryServiceFactory::GetInstance()
        ->GetForProfile(profile(), ServiceAccessType::IMPLICIT_ACCESS)
        ->AddPage(url, base::Time::Now() - base::Hours(25),
                  history::SOURCE_BROWSED);
    CheckSiteFamiliarity(url, FamiliarityVerdict::kFamiliar);
  }

 protected:
  virtual bool ShouldEnableSiteFamiliarityFeature() = 0;

  virtual bool ShouldForceSitePerProcess() { return true; }

 private:
  safe_browsing::TestSafeBrowsingServiceFactory factory_;
  base::test::ScopedFeatureList feature_list_;
};

// Test harness for integration tests which enable the "disable JavaScript
// optimization for unfamiliar sites" feature.
class JavascriptOptimizerBrowserTest_UseSiteFamiliarity
    : public JavascriptOptimizerBrowserTest_UseSiteFamiliarityBase {
 public:
  JavascriptOptimizerBrowserTest_UseSiteFamiliarity() = default;
  ~JavascriptOptimizerBrowserTest_UseSiteFamiliarity() override = default;

 protected:
  bool ShouldEnableSiteFamiliarityFeature() override { return true; }
};

IN_PROC_BROWSER_TEST_F(JavascriptOptimizerBrowserTest_UseSiteFamiliarity,
                       ExpectOptimizationEnabledFamiliarSite) {
  const GURL kTestUrl =
      embedded_https_test_server().GetURL("a.com", "/simple.html");
  MarkAsFamiliar(kTestUrl);
  ASSERT_TRUE(content::NavigateToURL(web_contents(), kTestUrl));
  EXPECT_FALSE(AreV8OptimizationsDisabledOnActiveWebContents());
}

IN_PROC_BROWSER_TEST_F(JavascriptOptimizerBrowserTest_UseSiteFamiliarity,
                       ExpectOptimizationDisabledForUnfamiliarSite) {
  const GURL kTestUrl =
      embedded_https_test_server().GetURL("a.com", "/simple.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), kTestUrl));
  CheckUnfamiliarSite(/*expect_v8_optimizations_enabled=*/false);
}

// Test that if there is a content-setting-exception to enable v8-optimizers
// for a specific site but the site is unfamiliar due to the heuristic that
// the content-setting-exception takes precedence.
IN_PROC_BROWSER_TEST_F(JavascriptOptimizerBrowserTest_UseSiteFamiliarity,
                       ExpectContentSettingExceptionTakesPrecedence) {
  const GURL kTestUrl =
      embedded_https_test_server().GetURL("a.com", "/simple.html");
  const ContentSettingsPattern kTestOriginPattern =
      ContentSettingsPattern::FromString("https://a.com/");
  auto* map = HostContentSettingsMapFactory::GetForProfile(profile());
  map->SetContentSettingCustomScope(kTestOriginPattern,
                                    ContentSettingsPattern::Wildcard(),
                                    ContentSettingsType::JAVASCRIPT_OPTIMIZER,
                                    ContentSetting::CONTENT_SETTING_ALLOW);

  ASSERT_TRUE(content::NavigateToURL(web_contents(), kTestUrl));
  CheckSiteFamiliarity(kTestUrl, FamiliarityVerdict::kUnfamiliar);
  EXPECT_FALSE(AreV8OptimizationsDisabledOnActiveWebContents());
}

// Check that v8-optimization is enabled for chrome:// URLs.
IN_PROC_BROWSER_TEST_F(JavascriptOptimizerBrowserTest_UseSiteFamiliarity,
                       ExpectOptimizationEnabledForChromeScheme) {
  ASSERT_TRUE(content::NavigateToURL(web_contents(), GURL("chrome://version")));
  EXPECT_FALSE(AreV8OptimizationsDisabledOnActiveWebContents());
}

// TODO(crbug.com/460621062): Re-enable the test
IN_PROC_BROWSER_TEST_F(
    JavascriptOptimizerBrowserTest_UseSiteFamiliarity,
    DISABLED_ExpectOptimizationsEnabledInSpeculativeRenderFrameHost) {
  // TODO(crbug.com/452130797): Determine desired behavior for this test case.

  const GURL kTestUrl = embedded_https_test_server().GetURL(
      "a.com", "/infinitely_loading_image.html");
  content::TestNavigationManager navigation_manager(web_contents(), kTestUrl);
  web_contents()->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(kTestUrl));
  navigation_manager.WaitForSpeculativeRenderFrameHostCreation();
  content::RenderFrameHost* speculative_render_frame_host =
      navigation_manager.GetCreatedSpeculativeRFH();
  EXPECT_FALSE(speculative_render_frame_host->GetProcess()
                   ->AreV8OptimizationsDisabled());
}

IN_PROC_BROWSER_TEST_F(JavascriptOptimizerBrowserTest_UseSiteFamiliarity,
                       ExpectOptimizationsEnabledInSpareRenderer) {
  // TODO(crbug.com/452689705): Consider creating both: (1) spare renderer
  // processes with v8-optimizer enabled and (2) spare renderer processes with
  // v8-optimizer disabled.

  auto& spare_manager = content::SpareRenderProcessHostManager::Get();
  spare_manager.WarmupSpare(profile());
  ASSERT_EQ(spare_manager.GetSpares().size(), 1u);
  EXPECT_FALSE(spare_manager.GetSpares()[0]->AreV8OptimizationsDisabled());
}

// Check that v8-optimizer is enabled if <iframe> URL is familiar.
IN_PROC_BROWSER_TEST_F(JavascriptOptimizerBrowserTest_UseSiteFamiliarity,
                       ExpectOptimizationEnabledForFamiliarIframe) {
  const GURL kTestUrl =
      embedded_https_test_server().GetURL("a.com", "/iframe_cross_site.html");
  const url::Origin kChildOrigin =
      url::Origin::Create(embedded_https_test_server().GetURL("b.com", "/"));

  MarkAsFamiliar(kChildOrigin.GetURL());
  CheckSiteFamiliarity(kTestUrl, FamiliarityVerdict::kUnfamiliar);
  ASSERT_TRUE(content::NavigateToURL(web_contents(), kTestUrl));

  EXPECT_TRUE(AreV8OptimizationsDisabledOnActiveWebContents());
  content::RenderFrameHost* child_frame =
      content::ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);
  EXPECT_EQ(kChildOrigin, child_frame->GetLastCommittedOrigin());
  EXPECT_FALSE(AreV8OptimizationsDisabledForRenderFrame(child_frame));
}

// Check that v8-optimizer is disabled if <iframe> URL is unfamiliar.
IN_PROC_BROWSER_TEST_F(JavascriptOptimizerBrowserTest_UseSiteFamiliarity,
                       ExpectOptimizationDisabledForUnfamiliarIframe) {
  const GURL kTestUrl =
      embedded_https_test_server().GetURL("a.com", "/iframe_cross_site.html");
  const url::Origin kChildOrigin =
      url::Origin::Create(embedded_https_test_server().GetURL("b.com", "/"));

  MarkAsFamiliar(kTestUrl);
  CheckSiteFamiliarity(kChildOrigin.GetURL(), FamiliarityVerdict::kUnfamiliar);
  ASSERT_TRUE(content::NavigateToURL(web_contents(), kTestUrl));

  EXPECT_FALSE(AreV8OptimizationsDisabledOnActiveWebContents());
  content::RenderFrameHost* child_frame =
      content::ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);
  EXPECT_EQ(kChildOrigin, child_frame->GetLastCommittedOrigin());
  EXPECT_TRUE(AreV8OptimizationsDisabledForRenderFrame(child_frame));
}

// Check that v8-optimization is disabled for data: <iframe> when
// the data:// URL is navigated to from a cross-origin <iframe> that also has v8
// optimizations disabled.
IN_PROC_BROWSER_TEST_F(JavascriptOptimizerBrowserTest_UseSiteFamiliarity,
                       ExpectOptimizationDisabledForDataUrlIframe) {
  const GURL kTestUrl =
      embedded_https_test_server().GetURL("a.com", "/iframe_cross_site.html");
  const GURL kDataUrl = GURL("data:,hello%20world");

  MarkAsFamiliar(kTestUrl);
  ASSERT_TRUE(content::NavigateToURL(web_contents(), kTestUrl));
  EXPECT_FALSE(AreV8OptimizationsDisabledOnActiveWebContents());

  content::RenderFrameHost* child_frame =
      content::ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(content::NavigateToURLFromRenderer(child_frame, kDataUrl));

  EXPECT_FALSE(AreV8OptimizationsDisabledOnActiveWebContents());
  child_frame = content::ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);
  EXPECT_TRUE(child_frame->GetProcess()->AreV8OptimizationsDisabled());
}

// Check that site-familiarity does not impede a data:// URL iframe from sharing
// a render process with the iframe parent.
IN_PROC_BROWSER_TEST_F(JavascriptOptimizerBrowserTest_UseSiteFamiliarity,
                       ExpectDataUrlIframeSameProcessAsParent) {
  const GURL kTestUrl =
      embedded_https_test_server().GetURL("a.com", "/iframe_data_url.html");
  MarkAsFamiliar(kTestUrl);
  ASSERT_TRUE(content::NavigateToURL(web_contents(), kTestUrl));

  // Parent frame is familiar. V8-optimization should be enabled.
  EXPECT_FALSE(AreV8OptimizationsDisabledOnActiveWebContents());

  // Child frame should use same render process as main frame.
  content::RenderFrameHost* child_frame =
      content::ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);
  ASSERT_EQ(child_frame->GetLastCommittedURL().scheme(), "data");
  EXPECT_EQ(child_frame->GetProcess(),
            web_contents()->GetPrimaryMainFrame()->GetProcess());
}

class JavascriptOptimizerBrowserTest_UseSiteFamiliarity_DisableSiteIsolation
    : public JavascriptOptimizerBrowserTest_UseSiteFamiliarity {
 public:
  JavascriptOptimizerBrowserTest_UseSiteFamiliarity_DisableSiteIsolation() =
      default;
  ~JavascriptOptimizerBrowserTest_UseSiteFamiliarity_DisableSiteIsolation()
      override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    JavascriptOptimizerBrowserTest_UseSiteFamiliarity::SetUpCommandLine(
        command_line);
    command_line->AppendSwitch(switches::kDisableSiteIsolation);
  }

  bool ShouldForceSitePerProcess() override { return false; }
};

// Check that site-familiarity is ignored when site-isolation is disabled.
// TODO(crbug.com/454006392): Determine desired site-familiarity behavior for
// unlocked processes if the site-familiarity feature is launched on Android.
IN_PROC_BROWSER_TEST_F(
    JavascriptOptimizerBrowserTest_UseSiteFamiliarity_DisableSiteIsolation,
    ExpectIgnoreFamiliarityWhenSiteIsolationDisabled) {
  const GURL kTestUrl =
      embedded_https_test_server().GetURL("a.com", "/simple.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), kTestUrl));
  CheckUnfamiliarSite(/*expect_v8_optimizations_enabled=*/true);
  EXPECT_FALSE(AreV8OptimizationsDisabledOnActiveWebContents());
}

// Test harness for integration tests which disable the "disable JavaScript
// optimization for unfamiliar sites" feature.
class JavascriptOptimizerBrowserTest_DoNotUseSiteFamiliarity
    : public JavascriptOptimizerBrowserTest_UseSiteFamiliarityBase {
 public:
  JavascriptOptimizerBrowserTest_DoNotUseSiteFamiliarity() = default;
  ~JavascriptOptimizerBrowserTest_DoNotUseSiteFamiliarity() override = default;

 protected:
  bool ShouldEnableSiteFamiliarityFeature() override { return false; }
};

IN_PROC_BROWSER_TEST_F(JavascriptOptimizerBrowserTest_DoNotUseSiteFamiliarity,
                       ExpectIgnoreFamiliarity_OptimizerAllowedByDefault) {
  auto* map = HostContentSettingsMapFactory::GetForProfile(profile());
  map->SetDefaultContentSetting(ContentSettingsType::JAVASCRIPT_OPTIMIZER,
                                ContentSetting::CONTENT_SETTING_ALLOW);
  CheckUnfamiliarSite(/*expect_v8_optimizations_enabled=*/true);
}

IN_PROC_BROWSER_TEST_F(JavascriptOptimizerBrowserTest_DoNotUseSiteFamiliarity,
                       ExpectIgnoreFamiliarity_OptimizerBlockedByDefault) {
  auto* map = HostContentSettingsMapFactory::GetForProfile(profile());
  map->SetDefaultContentSetting(ContentSettingsType::JAVASCRIPT_OPTIMIZER,
                                ContentSetting::CONTENT_SETTING_BLOCK);
  CheckUnfamiliarSite(/*expect_v8_optimizations_enabled=*/false);
}

#if !BUILDFLAG(IS_ANDROID)
class JavascriptOptimizerOmnibarIconBrowserTest
    : public PageActionInteractiveTestMixin<InteractiveBrowserTest> {
 public:
  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    content::SetupCrossSiteRedirector(&embedded_https_test_server());

    embedded_https_test_server().SetCertHostnames(
        {"a.com", "*.a.com", "b.com", "*.b.com", "unrelated.com"});
    ASSERT_TRUE(embedded_https_test_server().Start());
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  Profile* profile() { return browser()->profile(); }

  bool AreV8OptimizationsDisabledForRenderFrame(content::RenderFrameHost* rfh) {
    return rfh->GetProcess()->AreV8OptimizationsDisabled();
  }

  bool AreV8OptimizationsDisabledOnActiveWebContents() {
    return AreV8OptimizationsDisabledForRenderFrame(
        web_contents()->GetPrimaryMainFrame());
  }

  // Returns true iff the JS Optimizations omnibar icon is visible.
  bool IsOmnibarIconVisible() {
    const auto* view = BrowserView::GetBrowserViewForBrowser(browser())
                           ->toolbar_button_provider()
                           ->GetPageActionView(kActionShowJsOptimizationsIcon);
    return view && view->GetVisible();
  }

  using PageActionInteractiveTestMixin::WaitForPageActionButtonVisible;
};

class JavascriptOptimizerOmnibarIconBrowserTest_WithFlag
    : public JavascriptOptimizerOmnibarIconBrowserTest {
 public:
  JavascriptOptimizerOmnibarIconBrowserTest_WithFlag() {
    feature_list_.InitWithFeatures(
        {content_settings::features::kBlockV8OptimizerOnUnfamiliarSitesSetting},
        {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(JavascriptOptimizerOmnibarIconBrowserTest_WithFlag,
                       IconShowsWhenOptimizationsDisabled) {
  auto* map = HostContentSettingsMapFactory::GetForProfile(profile());
  map->SetDefaultContentSetting(ContentSettingsType::JAVASCRIPT_OPTIMIZER,
                                ContentSetting::CONTENT_SETTING_BLOCK);

  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), embedded_https_test_server().GetURL("/simple.html")));
  ASSERT_TRUE(AreV8OptimizationsDisabledOnActiveWebContents());
  RunTestSequence(
      WaitForPageActionButtonVisible(kActionShowJsOptimizationsIcon));
  EXPECT_TRUE(IsOmnibarIconVisible());
}

IN_PROC_BROWSER_TEST_F(JavascriptOptimizerOmnibarIconBrowserTest_WithFlag,
                       IconDoesNotShowWhenOptimizationsNotDisabled) {
  auto* map = HostContentSettingsMapFactory::GetForProfile(profile());
  map->SetDefaultContentSetting(ContentSettingsType::JAVASCRIPT_OPTIMIZER,
                                ContentSetting::CONTENT_SETTING_ALLOW);

  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), embedded_https_test_server().GetURL("/simple.html")));
  ASSERT_FALSE(AreV8OptimizationsDisabledOnActiveWebContents());
  RunTestSequence(
      WaitForPageActionChipNotVisible(kActionShowJsOptimizationsIcon));
  EXPECT_FALSE(IsOmnibarIconVisible());
}

IN_PROC_BROWSER_TEST_F(
    JavascriptOptimizerOmnibarIconBrowserTest_WithFlag,
    IconShowsWhenNavigatingToPageWhereOptimizationsDisabled) {
  auto* map = HostContentSettingsMapFactory::GetForProfile(profile());
  // Optimizations enabled for all except a.com
  map->SetDefaultContentSetting(ContentSettingsType::JAVASCRIPT_OPTIMIZER,
                                ContentSetting::CONTENT_SETTING_ALLOW);
  map->SetContentSettingCustomScope(
      ContentSettingsPattern::FromString("https://a.com:*"),
      ContentSettingsPattern::FromString("*"),
      ContentSettingsType::JAVASCRIPT_OPTIMIZER,
      ContentSetting::CONTENT_SETTING_BLOCK);

  // At first, optimizations not disabled, so icon is not visible.
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_https_test_server().GetURL("b.com", "/simple.html")));
  ASSERT_FALSE(AreV8OptimizationsDisabledOnActiveWebContents());
  RunTestSequence(
      WaitForPageActionChipNotVisible(kActionShowJsOptimizationsIcon));
  EXPECT_FALSE(IsOmnibarIconVisible());
  // After navigating to a.com, icon is visible.
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_https_test_server().GetURL("a.com", "/simple.html")));
  ASSERT_TRUE(AreV8OptimizationsDisabledOnActiveWebContents());
  RunTestSequence(
      WaitForPageActionButtonVisible(kActionShowJsOptimizationsIcon));
  EXPECT_TRUE(IsOmnibarIconVisible());
}

IN_PROC_BROWSER_TEST_F(
    JavascriptOptimizerOmnibarIconBrowserTest_WithFlag,
    IconDisappearsWhenNavigatingToPageWhereOptimizationsNotDisabled) {
  auto* map = HostContentSettingsMapFactory::GetForProfile(profile());
  // Optimizations enabled for all except a.com
  map->SetDefaultContentSetting(ContentSettingsType::JAVASCRIPT_OPTIMIZER,
                                ContentSetting::CONTENT_SETTING_ALLOW);
  map->SetContentSettingCustomScope(
      ContentSettingsPattern::FromString("https://a.com:*"),
      ContentSettingsPattern::FromString("*"),
      ContentSettingsType::JAVASCRIPT_OPTIMIZER,
      ContentSetting::CONTENT_SETTING_BLOCK);

  // At first, optimizations disabled, so icon is visible.
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_https_test_server().GetURL("a.com", "/simple.html")));
  ASSERT_TRUE(AreV8OptimizationsDisabledOnActiveWebContents());
  RunTestSequence(
      WaitForPageActionButtonVisible(kActionShowJsOptimizationsIcon));
  EXPECT_TRUE(IsOmnibarIconVisible());
  // After navigating to b.com, icon is not visible.
  ASSERT_TRUE(content::NavigateToURL(
      web_contents(),
      embedded_https_test_server().GetURL("b.com", "/simple.html")));
  ASSERT_FALSE(AreV8OptimizationsDisabledOnActiveWebContents());
  RunTestSequence(
      WaitForPageActionChipNotVisible(kActionShowJsOptimizationsIcon));
  EXPECT_FALSE(IsOmnibarIconVisible());
}

class JavascriptOptimizerOmnibarIconBrowserTest_WithoutFlag
    : public JavascriptOptimizerOmnibarIconBrowserTest {};

IN_PROC_BROWSER_TEST_F(JavascriptOptimizerOmnibarIconBrowserTest_WithoutFlag,
                       IconDoesNotShowWhenFlagNotEnabled) {
  auto* map = HostContentSettingsMapFactory::GetForProfile(profile());
  map->SetDefaultContentSetting(ContentSettingsType::JAVASCRIPT_OPTIMIZER,
                                ContentSetting::CONTENT_SETTING_BLOCK);

  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), embedded_https_test_server().GetURL("/simple.html")));
  // V8 optimizations are disabled, but omnibar icon is not visible.
  ASSERT_TRUE(AreV8OptimizationsDisabledOnActiveWebContents());
  const auto* icon_view =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->toolbar_button_provider()
          ->GetPageActionView(kActionShowJsOptimizationsIcon);
  // There is no view initialized because the flag is disabled.
  ASSERT_EQ(icon_view, nullptr);
}
class JavascriptOptimizerBubbleBrowserTest
    : public JavascriptOptimizerOmnibarIconBrowserTest {
 public:
  JavascriptOptimizerBubbleBrowserTest() {
    feature_list_.InitWithFeatures(
        {content_settings::features::kBlockV8OptimizerOnUnfamiliarSitesSetting},
        {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// JS optimizations disabled by enterprise policy.
class JavascriptOptimizerBubbleBrowserTest_EnterprisePolicy
    : public JavascriptOptimizerBubbleBrowserTest {};

IN_PROC_BROWSER_TEST_F(JavascriptOptimizerBubbleBrowserTest_EnterprisePolicy,
                       BubbleShowsOnClick) {
  auto* map = HostContentSettingsMapFactory::GetForProfile(profile());
  map->SetDefaultContentSetting(ContentSettingsType::JAVASCRIPT_OPTIMIZER,
                                ContentSetting::CONTENT_SETTING_BLOCK);

  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), embedded_https_test_server().GetURL("/simple.html")));
  ASSERT_TRUE(AreV8OptimizationsDisabledOnActiveWebContents());
  ASSERT_TRUE(IsOmnibarIconVisible());

  // TODO(crbug.com/462425975): Complete implementation of this test.
  // Click on icon.
  // Assert that bubble is visible.
  // Assert that button is not visible.
}

// JS optimizations disabled not by enterprise policy.
class JavascriptOptimizerBubbleBrowserTest_NotFromEnterprisePolicy
    : public JavascriptOptimizerBubbleBrowserTest {};

IN_PROC_BROWSER_TEST_F(
    JavascriptOptimizerBubbleBrowserTest_NotFromEnterprisePolicy,
    BubbleShowsOnClick) {
  auto* map = HostContentSettingsMapFactory::GetForProfile(profile());
  map->SetDefaultContentSetting(ContentSettingsType::JAVASCRIPT_OPTIMIZER,
                                ContentSetting::CONTENT_SETTING_BLOCK);

  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), embedded_https_test_server().GetURL("/simple.html")));
  ASSERT_TRUE(AreV8OptimizationsDisabledOnActiveWebContents());
  ASSERT_TRUE(IsOmnibarIconVisible());

  // TODO(crbug.com/462425975): Complete implementation of this test.
  // Click on icon.
  // Assert that bubble is visible.
  // Assert that button is visible.
}

IN_PROC_BROWSER_TEST_F(
    JavascriptOptimizerBubbleBrowserTest_NotFromEnterprisePolicy,
    AfterEnablingOptimizationsIconIsHidden) {
  auto* map = HostContentSettingsMapFactory::GetForProfile(profile());
  map->SetDefaultContentSetting(ContentSettingsType::JAVASCRIPT_OPTIMIZER,
                                ContentSetting::CONTENT_SETTING_BLOCK);

  ASSERT_TRUE(content::NavigateToURL(
      web_contents(), embedded_https_test_server().GetURL("/simple.html")));
  ASSERT_TRUE(AreV8OptimizationsDisabledOnActiveWebContents());
  ASSERT_TRUE(IsOmnibarIconVisible());

  // TODO(crbug.com/462425975): Complete implementation of this test.
  // Click on icon.
  // Assert that bubble is visible.
  // Assert that button is visible.
  // Click on button.
  // Open new tab and go to the same site.
  // ASSERT_FALSE(AreV8OptimizationsDisabledOnActiveWebContents());
  // ASSERT_FALSE(IsOmnibarIconVisible());
}

#endif  // !BUILDFLAG(IS_ANDROID)
