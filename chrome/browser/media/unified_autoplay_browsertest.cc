// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "media/base/media_switches.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/blink/public/mojom/autoplay/autoplay.mojom.h"

namespace {

static constexpr char const kFramedTestPagePath[] =
    "/media/autoplay_iframe.html";

static constexpr char const kTestPagePath[] = "/media/unified_autoplay.html";

class ChromeContentBrowserClientOverrideWebAppScope
    : public ChromeContentBrowserClient {
 public:
  ChromeContentBrowserClientOverrideWebAppScope() = default;
  ~ChromeContentBrowserClientOverrideWebAppScope() override = default;

  void OverrideWebkitPrefs(
      content::WebContents* web_contents,
      blink::web_pref::WebPreferences* web_prefs) override {
    ChromeContentBrowserClient::OverrideWebkitPrefs(web_contents, web_prefs);

    web_prefs->web_app_scope = web_app_scope_;
  }

  void set_web_app_scope(const GURL& web_app_scope) {
    web_app_scope_ = web_app_scope;
  }

 private:
  GURL web_app_scope_;
};

}  // anonymous namespace

// Integration tests for the unified autoplay policy that require the //chrome
// layer.
// These tests are called "UnifiedAutoplayBrowserTest" in order to avoid name
// conflict with "AutoplayBrowserTest" in extensions code.
class UnifiedAutoplayBrowserTest : public InProcessBrowserTest {
 public:
  UnifiedAutoplayBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(media::kUnifiedAutoplay);
  }

  ~UnifiedAutoplayBrowserTest() override = default;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());

    InProcessBrowserTest::SetUpOnMainThread();
  }

  content::WebContents* OpenNewTab(const GURL& url, bool from_context_menu) {
    return OpenInternal(
        url, from_context_menu, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        false /* is_renderer_initiated */, true /* user_gesture */);
  }

  content::WebContents* OpenNewWindow(const GURL& url, bool from_context_menu) {
    return OpenInternal(
        url, from_context_menu, WindowOpenDisposition::NEW_WINDOW,
        false /* is_renderer_initiated */, true /* user_gesture */);
  }

  content::WebContents* OpenFromRenderer(const GURL& url, bool user_gesture) {
    return OpenInternal(url, false /* from_context_menu */,
                        WindowOpenDisposition::NEW_FOREGROUND_TAB,
                        true /* is_renderer_initiated */, user_gesture);
  }

  bool AttemptPlay(content::WebContents* web_contents) {
    return content::EvalJs(web_contents, "attemptPlay();",
                           content::EXECUTE_SCRIPT_NO_USER_GESTURE)
        .ExtractBool();
  }

  bool NavigateInRenderer(content::WebContents* web_contents, const GURL& url) {
    content::TestNavigationObserver observer(web_contents);

    bool result =
        content::ExecJs(web_contents, "window.location = '" + url.spec() + "';",
                        content::EXECUTE_SCRIPT_NO_USER_GESTURE);

    if (result)
      observer.Wait();
    return result;
  }

  void SetAutoplayForceAllowFlag(content::RenderFrameHost* rfh,
                                 const GURL& url) {
    mojo::AssociatedRemote<blink::mojom::AutoplayConfigurationClient> client;
    rfh->GetRemoteAssociatedInterfaces()->GetInterface(&client);
    client->AddAutoplayFlags(url::Origin::Create(url),
                             blink::mojom::kAutoplayFlagForceAllow);
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 private:
  content::WebContents* OpenInternal(const GURL& url,
                                     bool from_context_menu,
                                     WindowOpenDisposition disposition,
                                     bool is_renderer_initiated,
                                     bool user_gesture) {
    content::WebContents* active_contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    content::Referrer referrer(active_contents->GetLastCommittedURL(),
                               network::mojom::ReferrerPolicy::kAlways);

    content::OpenURLParams open_url_params(
        url, referrer, disposition, ui::PAGE_TRANSITION_LINK,
        is_renderer_initiated, from_context_menu);

    open_url_params.initiator_origin =
        active_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin();
    open_url_params.source_render_process_id =
        active_contents->GetPrimaryMainFrame()->GetProcess()->GetID();
    open_url_params.source_render_frame_id =
        active_contents->GetPrimaryMainFrame()->GetRoutingID();
    open_url_params.user_gesture = user_gesture;

    return active_contents->OpenURL(open_url_params,
                                    /*navigation_handle_callback=*/{});
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(UnifiedAutoplayBrowserTest, OpenSameOriginOutsideMenu) {
  const GURL kTestPageUrl = embedded_test_server()->GetURL(kTestPagePath);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kTestPageUrl));

  content::WebContents* new_contents = OpenNewTab(kTestPageUrl, false);
  EXPECT_TRUE(content::WaitForLoadStop(new_contents));

  EXPECT_FALSE(AttemptPlay(new_contents));
}

IN_PROC_BROWSER_TEST_F(UnifiedAutoplayBrowserTest, OpenSameOriginFromMenu) {
  const GURL kTestPageUrl = embedded_test_server()->GetURL(kTestPagePath);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kTestPageUrl));

  content::WebContents* new_contents = OpenNewTab(kTestPageUrl, true);
  EXPECT_TRUE(content::WaitForLoadStop(new_contents));

  EXPECT_TRUE(AttemptPlay(new_contents));
}

IN_PROC_BROWSER_TEST_F(UnifiedAutoplayBrowserTest, OpenCrossOriginFromMenu) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("foo.example.com", kTestPagePath)));

  content::WebContents* new_contents = OpenNewTab(
      embedded_test_server()->GetURL("bar.example.com", kTestPagePath), true);
  EXPECT_TRUE(content::WaitForLoadStop(new_contents));

  EXPECT_TRUE(AttemptPlay(new_contents));
}

IN_PROC_BROWSER_TEST_F(UnifiedAutoplayBrowserTest, OpenCrossDomainFromMenu) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kTestPagePath)));

  content::WebContents* new_contents = OpenNewTab(
      embedded_test_server()->GetURL("example.com", kTestPagePath), true);
  EXPECT_TRUE(content::WaitForLoadStop(new_contents));

  EXPECT_FALSE(AttemptPlay(new_contents));
}

IN_PROC_BROWSER_TEST_F(UnifiedAutoplayBrowserTest, OpenWindowFromContextMenu) {
  const GURL kTestPageUrl = embedded_test_server()->GetURL(kTestPagePath);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kTestPageUrl));

  content::WebContents* new_contents = OpenNewTab(kTestPageUrl, true);
  EXPECT_TRUE(content::WaitForLoadStop(new_contents));

  EXPECT_TRUE(AttemptPlay(new_contents));
}

IN_PROC_BROWSER_TEST_F(UnifiedAutoplayBrowserTest, OpenWindowNotContextMenu) {
  const GURL kTestPageUrl = embedded_test_server()->GetURL(kTestPagePath);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kTestPageUrl));

  content::WebContents* new_contents = OpenNewTab(kTestPageUrl, false);
  EXPECT_TRUE(content::WaitForLoadStop(new_contents));

  EXPECT_FALSE(AttemptPlay(new_contents));
}

IN_PROC_BROWSER_TEST_F(UnifiedAutoplayBrowserTest, OpenFromRendererGesture) {
  const GURL kTestPageUrl = embedded_test_server()->GetURL(kTestPagePath);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kTestPageUrl));

  content::WebContents* new_contents = OpenFromRenderer(kTestPageUrl, true);
  EXPECT_TRUE(content::WaitForLoadStop(new_contents));

  EXPECT_TRUE(AttemptPlay(new_contents));
}

IN_PROC_BROWSER_TEST_F(UnifiedAutoplayBrowserTest, OpenFromRendererNoGesture) {
  const GURL kTestPageUrl = embedded_test_server()->GetURL(kTestPagePath);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kTestPageUrl));

  content::WebContents* new_contents = OpenFromRenderer(kTestPageUrl, false);
  EXPECT_EQ(nullptr, new_contents);
}

IN_PROC_BROWSER_TEST_F(UnifiedAutoplayBrowserTest, NoBypassUsingAutoplayFlag) {
  const GURL kTestPageUrl = embedded_test_server()->GetURL(kTestPagePath);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kTestPageUrl));

  EXPECT_FALSE(AttemptPlay(GetWebContents()));
}

IN_PROC_BROWSER_TEST_F(UnifiedAutoplayBrowserTest, BypassUsingAutoplayFlag) {
  const GURL kTestPageUrl = embedded_test_server()->GetURL(kTestPagePath);

  content::TestNavigationManager navigation_manager(GetWebContents(),
                                                    kTestPageUrl);
  content::NavigationController::LoadURLParams params(kTestPageUrl);
  params.transition_type = ui::PAGE_TRANSITION_LINK;
  params.frame_tree_node_id =
      GetWebContents()->GetPrimaryMainFrame()->GetFrameTreeNodeId();
  GetWebContents()->GetController().LoadURLWithParams(params);
  EXPECT_TRUE(navigation_manager.WaitForResponse());

  // Set the flag on the RenderFrameHost we're navigating to as well, in case
  // we commit in a different RenderFrameHsot.
  SetAutoplayForceAllowFlag(
      navigation_manager.GetNavigationHandle()->GetRenderFrameHost(),
      kTestPageUrl);
  navigation_manager.ResumeNavigation();
  EXPECT_TRUE(navigation_manager.WaitForNavigationFinished());
  EXPECT_TRUE(content::WaitForLoadStop(GetWebContents()));

  EXPECT_TRUE(AttemptPlay(GetWebContents()));
}

IN_PROC_BROWSER_TEST_F(UnifiedAutoplayBrowserTest,
                       BypassUsingAutoplayFlag_SameDocument) {
  const GURL kTestPageUrl = embedded_test_server()->GetURL(kTestPagePath);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kTestPageUrl));
  SetAutoplayForceAllowFlag(GetWebContents()->GetPrimaryMainFrame(),
                            kTestPageUrl);

  // Simulate a same document navigation by navigating to #test.
  GURL::Replacements replace_ref;
  replace_ref.SetRefStr("test");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), kTestPageUrl.ReplaceComponents(replace_ref)));

  EXPECT_TRUE(AttemptPlay(GetWebContents()));
}

IN_PROC_BROWSER_TEST_F(UnifiedAutoplayBrowserTest, ForceWasActivated_Default) {
  const GURL kTestPageUrl = embedded_test_server()->GetURL(kTestPagePath);

  NavigateParams params(browser(), kTestPageUrl, ui::PAGE_TRANSITION_LINK);
  ui_test_utils::NavigateToURL(&params);

  EXPECT_FALSE(AttemptPlay(GetWebContents()));
}

IN_PROC_BROWSER_TEST_F(UnifiedAutoplayBrowserTest, ForceWasActivated_Yes) {
  const GURL kTestPageUrl = embedded_test_server()->GetURL(kTestPagePath);

  NavigateParams params(browser(), kTestPageUrl, ui::PAGE_TRANSITION_LINK);
  params.was_activated = blink::mojom::WasActivatedOption::kYes;
  ui_test_utils::NavigateToURL(&params);

  EXPECT_TRUE(AttemptPlay(GetWebContents()));
}

IN_PROC_BROWSER_TEST_F(UnifiedAutoplayBrowserTest,
                       Redirect_SameOrigin_WithGesture) {
  const GURL kRedirectPageUrl =
      embedded_test_server()->GetURL(kFramedTestPagePath);
  const GURL kTestPageUrl = embedded_test_server()->GetURL(kTestPagePath);

  NavigateParams params(browser(), kRedirectPageUrl, ui::PAGE_TRANSITION_LINK);
  params.was_activated = blink::mojom::WasActivatedOption::kYes;
  ui_test_utils::NavigateToURL(&params);

  EXPECT_TRUE(NavigateInRenderer(GetWebContents(), kTestPageUrl));
  EXPECT_EQ(kTestPageUrl, GetWebContents()->GetLastCommittedURL());
  EXPECT_TRUE(AttemptPlay(GetWebContents()));
}

IN_PROC_BROWSER_TEST_F(UnifiedAutoplayBrowserTest,
                       Redirect_SameOrigin_WithoutGesture) {
  const GURL kRedirectPageUrl =
      embedded_test_server()->GetURL(kFramedTestPagePath);
  const GURL kTestPageUrl = embedded_test_server()->GetURL(kTestPagePath);

  NavigateParams params(browser(), kRedirectPageUrl, ui::PAGE_TRANSITION_LINK);
  ui_test_utils::NavigateToURL(&params);

  EXPECT_TRUE(NavigateInRenderer(GetWebContents(), kTestPageUrl));
  EXPECT_EQ(kTestPageUrl, GetWebContents()->GetLastCommittedURL());
  EXPECT_FALSE(AttemptPlay(GetWebContents()));
}

IN_PROC_BROWSER_TEST_F(UnifiedAutoplayBrowserTest,
                       Redirect_CrossOrigin_WithGesture) {
  const GURL kRedirectPageUrl =
      embedded_test_server()->GetURL(kFramedTestPagePath);
  const GURL kTestPageUrl =
      embedded_test_server()->GetURL("foo.example.com", kTestPagePath);

  NavigateParams params(browser(), kRedirectPageUrl, ui::PAGE_TRANSITION_LINK);
  params.was_activated = blink::mojom::WasActivatedOption::kYes;
  ui_test_utils::NavigateToURL(&params);

  EXPECT_TRUE(NavigateInRenderer(GetWebContents(), kTestPageUrl));
  EXPECT_EQ(kTestPageUrl, GetWebContents()->GetLastCommittedURL());
  EXPECT_FALSE(AttemptPlay(GetWebContents()));
}

IN_PROC_BROWSER_TEST_F(UnifiedAutoplayBrowserTest,
                       Redirect_CrossOrigin_WithoutGesture) {
  const GURL kRedirectPageUrl =
      embedded_test_server()->GetURL(kFramedTestPagePath);
  const GURL kTestPageUrl =
      embedded_test_server()->GetURL("foo.example.com", kTestPagePath);

  NavigateParams params(browser(), kRedirectPageUrl, ui::PAGE_TRANSITION_LINK);
  ui_test_utils::NavigateToURL(&params);

  EXPECT_TRUE(NavigateInRenderer(GetWebContents(), kTestPageUrl));
  EXPECT_EQ(kTestPageUrl, GetWebContents()->GetLastCommittedURL());
  EXPECT_FALSE(AttemptPlay(GetWebContents()));
}

IN_PROC_BROWSER_TEST_F(UnifiedAutoplayBrowserTest,
                       MatchingWebAppScopeAllowsAutoplay_Origin) {
  GURL kTestPageUrl(
      embedded_test_server()->GetURL("example.com", kTestPagePath));

  ChromeContentBrowserClientOverrideWebAppScope browser_client;
  browser_client.set_web_app_scope(kTestPageUrl.DeprecatedGetOriginAsURL());

  content::ContentBrowserClient* old_browser_client =
      content::SetBrowserClientForTesting(&browser_client);

  GetWebContents()->OnWebPreferencesChanged();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kTestPageUrl));
  EXPECT_TRUE(content::WaitForLoadStop(GetWebContents()));

  EXPECT_TRUE(AttemptPlay(GetWebContents()));

  content::SetBrowserClientForTesting(old_browser_client);
}

IN_PROC_BROWSER_TEST_F(UnifiedAutoplayBrowserTest,
                       MatchingWebAppScopeAllowsAutoplay_Path) {
  GURL kTestPageUrl(
      embedded_test_server()->GetURL("example.com", kTestPagePath));

  ChromeContentBrowserClientOverrideWebAppScope browser_client;
  browser_client.set_web_app_scope(kTestPageUrl.GetWithoutFilename());

  content::ContentBrowserClient* old_browser_client =
      content::SetBrowserClientForTesting(&browser_client);

  GetWebContents()->OnWebPreferencesChanged();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kTestPageUrl));
  EXPECT_TRUE(content::WaitForLoadStop(GetWebContents()));

  EXPECT_TRUE(AttemptPlay(GetWebContents()));

  content::SetBrowserClientForTesting(old_browser_client);
}

IN_PROC_BROWSER_TEST_F(UnifiedAutoplayBrowserTest,
                       NotMatchingWebAppScopeDoesNotAllowAutoplay) {
  GURL kTestPageUrl(
      embedded_test_server()->GetURL("example.com", kTestPagePath));

  ChromeContentBrowserClientOverrideWebAppScope browser_client;
  browser_client.set_web_app_scope(GURL("http://www.foobar.com"));

  content::ContentBrowserClient* old_browser_client =
      content::SetBrowserClientForTesting(&browser_client);

  GetWebContents()->OnWebPreferencesChanged();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kTestPageUrl));
  EXPECT_TRUE(content::WaitForLoadStop(GetWebContents()));

  EXPECT_FALSE(AttemptPlay(GetWebContents()));

  content::SetBrowserClientForTesting(old_browser_client);
}

// Integration tests for the new unified autoplay sound settings UI.

class UnifiedAutoplaySettingBrowserTest : public UnifiedAutoplayBrowserTest {
 public:
  UnifiedAutoplaySettingBrowserTest() {
    scoped_feature_list_.InitWithFeatures({media::kAutoplayDisableSettings},
                                          {});
  }

  ~UnifiedAutoplaySettingBrowserTest() override = default;

  void SetUpOnMainThread() override {
    UnifiedAutoplayBrowserTest::SetUpOnMainThread();
  }

  bool AutoplayAllowed(const content::ToRenderFrameHost& adapter) {
    return content::EvalJs(adapter, "tryPlayback();",
                           content::EXECUTE_SCRIPT_NO_USER_GESTURE)
        .ExtractBool();
  }

  void NavigateFrameAndWait(content::RenderFrameHost* rfh, const GURL& url) {
    content::TestFrameNavigationObserver observer(rfh);
    content::NavigationController::LoadURLParams params(url);
    params.transition_type = ui::PAGE_TRANSITION_LINK;
    params.frame_tree_node_id = rfh->GetFrameTreeNodeId();
    content::WebContents::FromRenderFrameHost(rfh)
        ->GetController()
        .LoadURLWithParams(params);
    observer.Wait();
  }

  HostContentSettingsMap* GetSettingsMap() {
    return HostContentSettingsMapFactory::GetForProfile(
        Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
  }

  content::RenderFrameHost* main_frame() const {
    return web_contents()->GetPrimaryMainFrame();
  }

  content::RenderFrameHost* first_child() const {
    return ChildFrameAt(main_frame(), 0);
  }

 private:
  content::WebContents* web_contents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

// Flaky. See https://crbug.com/1101524.
IN_PROC_BROWSER_TEST_F(UnifiedAutoplaySettingBrowserTest, DISABLED_Allow) {
  GURL main_url(
      embedded_test_server()->GetURL("example.com", kFramedTestPagePath));
  GURL foo_url(embedded_test_server()->GetURL("foo.com", kFramedTestPagePath));

  GetSettingsMap()->SetContentSettingDefaultScope(
      main_url, main_url, ContentSettingsType::SOUND, CONTENT_SETTING_ALLOW);

  NavigateFrameAndWait(main_frame(), main_url);
  NavigateFrameAndWait(first_child(), foo_url);

  EXPECT_TRUE(AutoplayAllowed(main_frame()));
  EXPECT_TRUE(AutoplayAllowed(first_child()));

  // Simulate a same document navigation by navigating to #test.
  GURL::Replacements replace_ref;
  replace_ref.SetRefStr("test");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), main_url.ReplaceComponents(replace_ref)));

  EXPECT_TRUE(AutoplayAllowed(main_frame()));
  EXPECT_TRUE(AutoplayAllowed(first_child()));
}

IN_PROC_BROWSER_TEST_F(UnifiedAutoplaySettingBrowserTest, Allow_Wildcard) {
  GURL main_url(
      embedded_test_server()->GetURL("example.com", kFramedTestPagePath));
  GURL foo_url(embedded_test_server()->GetURL("foo.org", kFramedTestPagePath));
  GURL bar_url(embedded_test_server()->GetURL("bar.com", kFramedTestPagePath));

  // Set a wildcard allow sound setting for *.com.
  ContentSettingsPattern pattern(ContentSettingsPattern::FromString("[*.]com"));
  GetSettingsMap()->SetWebsiteSettingCustomScope(
      pattern, ContentSettingsPattern::Wildcard(), ContentSettingsType::SOUND,
      base::Value(CONTENT_SETTING_ALLOW));

  NavigateFrameAndWait(main_frame(), main_url);
  EXPECT_TRUE(AutoplayAllowed(main_frame()));

  NavigateFrameAndWait(main_frame(), foo_url);
  EXPECT_FALSE(AutoplayAllowed(main_frame()));

  NavigateFrameAndWait(main_frame(), bar_url);
  EXPECT_TRUE(AutoplayAllowed(main_frame()));
}

// Flaky. See https://crbug.com/1106521.
IN_PROC_BROWSER_TEST_F(UnifiedAutoplaySettingBrowserTest, DISABLED_Block) {
  GURL main_url(
      embedded_test_server()->GetURL("example.com", kFramedTestPagePath));
  GURL foo_url(embedded_test_server()->GetURL("foo.com", kFramedTestPagePath));

  GetSettingsMap()->SetContentSettingDefaultScope(
      main_url, main_url, ContentSettingsType::SOUND, CONTENT_SETTING_BLOCK);

  NavigateFrameAndWait(main_frame(), main_url);
  NavigateFrameAndWait(first_child(), foo_url);

  EXPECT_FALSE(AutoplayAllowed(main_frame()));
  EXPECT_FALSE(AutoplayAllowed(first_child()));
}

IN_PROC_BROWSER_TEST_F(UnifiedAutoplaySettingBrowserTest, Block_Wildcard) {
  GURL main_url(
      embedded_test_server()->GetURL("example.com", kFramedTestPagePath));
  GURL foo_url(embedded_test_server()->GetURL("foo.org", kFramedTestPagePath));
  GURL bar_url(embedded_test_server()->GetURL("bar.com", kFramedTestPagePath));

  // Set a wildcard block sound setting for *.com.
  ContentSettingsPattern pattern(ContentSettingsPattern::FromString("[*.]com"));
  GetSettingsMap()->SetWebsiteSettingCustomScope(
      pattern, ContentSettingsPattern::Wildcard(), ContentSettingsType::SOUND,
      base::Value(CONTENT_SETTING_BLOCK));

  GetSettingsMap()->SetContentSettingDefaultScope(
      foo_url, foo_url, ContentSettingsType::SOUND, CONTENT_SETTING_ALLOW);

  NavigateFrameAndWait(main_frame(), main_url);
  EXPECT_FALSE(AutoplayAllowed(main_frame()));

  NavigateFrameAndWait(main_frame(), foo_url);
  EXPECT_TRUE(AutoplayAllowed(main_frame()));

  NavigateFrameAndWait(main_frame(), bar_url);
  EXPECT_FALSE(AutoplayAllowed(main_frame()));
}

// Flaky. See https://crbug.com/1101524.
IN_PROC_BROWSER_TEST_F(UnifiedAutoplaySettingBrowserTest,
                       DISABLED_DefaultAllow) {
  GURL main_url(
      embedded_test_server()->GetURL("example.com", kFramedTestPagePath));
  GURL foo_url(embedded_test_server()->GetURL("foo.com", kFramedTestPagePath));

  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            GetSettingsMap()->GetContentSetting(main_url, main_url,
                                                ContentSettingsType::SOUND));

  NavigateFrameAndWait(main_frame(), main_url);
  NavigateFrameAndWait(first_child(), foo_url);

  EXPECT_FALSE(AutoplayAllowed(main_frame()));
  EXPECT_FALSE(AutoplayAllowed(first_child()));
}
