// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/extensions/settings_api_bubble_helpers.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/test/test_extension_dir.h"
#include "third_party/blink/public/common/features.h"

namespace extensions {

class OmniboxFocusInteractiveTest : public ExtensionBrowserTest {
 public:
  OmniboxFocusInteractiveTest() = default;
  ~OmniboxFocusInteractiveTest() override = default;

 protected:
  void WriteExtensionFile(const base::FilePath::StringType& filename,
                          std::string_view contents) {
    test_dir_.WriteFile(filename, contents);
  }

  const Extension* CreateAndLoadNtpReplacementExtension() {
    const char kManifest[] = R"(
        {
          "chrome_url_overrides": {
              "newtab": "ext_ntp.html"
          },
          "manifest_version": 2,
          "name": "NTP-replacement extension",
          "version": "1.0"
        } )";
    test_dir_.WriteManifest(kManifest);
    const Extension* extension = LoadExtension(test_dir_.UnpackedPath());
    if (!extension) {
      return nullptr;
    }

    // Prevent a focus-stealing focus bubble that warns the user that "An
    // extension has changed what page is shown when you open a new tab."
    ExtensionPrefs* prefs = ExtensionPrefs::Get(browser()->profile());
    prefs->UpdateExtensionPref(extension->id(),
                               kNtpOverridingExtensionAcknowledged,
                               base::Value(true));

    return extension;
  }

  void OpenNewTab() {
    chrome::NewTab(browser());
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    // Wait until chrome://newtab navigation finished.
    content::TestNavigationObserver nav_observer(web_contents);
    nav_observer.Wait();
  }

 private:
  TestExtensionDir test_dir_;
};

// Verify that setting window.location in an NTP-replacement extension results
// in the NTP web contents being focused - this is a regression test for
// https://crbug.com/1027719.  We expect the tab contents to be focused when
// navigating away from the NTP - this is what happens in the location
// assignment case.
IN_PROC_BROWSER_TEST_F(OmniboxFocusInteractiveTest,
                       NtpReplacementExtension_LocationAssignment) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Open the new tab, focus should be on the location bar.
  OpenNewTab();
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));

  // Install an extension that
  // 1) provides a replacement for chrome://newtab URL
  // 2) navigates away from the replacement
  WriteExtensionFile(FILE_PATH_LITERAL("ext_ntp.html"),
                     "<script src='ext_ntp.js'></script>");
  GURL final_ntp_url = embedded_test_server()->GetURL("/title1.html");
  WriteExtensionFile(FILE_PATH_LITERAL("ext_ntp.js"),
                     content::JsReplace("window.location = $1", final_ntp_url));
  const Extension* extension = CreateAndLoadNtpReplacementExtension();
  ASSERT_TRUE(extension);

  // Open the new tab, because of the NTP extension behavior, the focus should
  // move to the tab contents.
  OpenNewTab();
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(final_ntp_url, web_contents->GetLastCommittedURL());
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));
  // No test assertion about |web_contents->GetController().GetEntryCount()|,
  // because location assignment may still result in replacing the existing
  // history entry if the client-redirect heuristics kick-in.

  // Focus the location bar / omnibox.
  chrome::FocusLocationBar(browser());
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));

  // When the webpage calls replaceState, the focus should not be stolen from
  // the omnibox (replaceState is not distinguishable from the earlier
  // navigation above from the perspective of Browser::ScheduleUIUpdate).
  GURL replaced_url = embedded_test_server()->GetURL("/replacement");
  {
    content::TestFrameNavigationObserver nav_observer(
        web_contents->GetPrimaryMainFrame());
    ASSERT_TRUE(content::ExecJs(
        web_contents, "history.replaceState({}, '', '/replacement');"));
    nav_observer.Wait();
  }
  EXPECT_EQ(replaced_url, web_contents->GetLastCommittedURL());
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));
}

// Verify that navigating via chrome.tabs.update does not steal the focus from
// the omnibox.  This is a regression test for https://crbug.com/1085779.
IN_PROC_BROWSER_TEST_F(OmniboxFocusInteractiveTest,
                       NtpReplacementExtension_TabsUpdate) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Open the new tab, focus should be on the location bar.
  OpenNewTab();
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));

  // Install an extension that provides a replacement for chrome://newtab URL.
  WriteExtensionFile(FILE_PATH_LITERAL("ext_ntp.html"),
                     "<body>NTP replacement extension</body>");
  const Extension* extension = CreateAndLoadNtpReplacementExtension();
  ASSERT_TRUE(extension);

  // Open the new tab.
  OpenNewTab();

  // Verify that ext_ntp.html is loaded in place of the NTP and that the omnibox
  // is focused.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ("NTP replacement extension",
            content::EvalJs(web_contents, "document.body.innerText"));
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));

  // Use the chrome.tabs.update API to navigate to a http URL.
  GURL final_ntp_url = embedded_test_server()->GetURL("/title1.html");
  const char kTabsUpdateTemplate[] = R"(
      const url = $1;
      chrome.tabs.getCurrent(function(tab) {
          chrome.tabs.update(tab.id, { "url": url });
      });
  )";
  content::TestFrameNavigationObserver nav_observer(
      web_contents->GetPrimaryMainFrame());
  content::ExecuteScriptAsync(
      web_contents, content::JsReplace(kTabsUpdateTemplate, final_ntp_url));
  nav_observer.Wait();
  EXPECT_EQ(2, web_contents->GetController().GetEntryCount());
  EXPECT_EQ(final_ntp_url,
            web_contents->GetController().GetLastCommittedEntry()->GetURL());

  // Verify that chrome.tabs.update didn't make the focus move away from the
  // omnibox.
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));
}

// Verify that calling window.location.replace in an NTP-replacement extension
// results in the NTP web contents being focused.  See also
// https://crbug.com/1027719 (which talks about a similar, but a slightly
// different scenario of assigning to window.location).  We expect the tab
// contents to be focused when navigating away from the NTP - this is what
// happens in the location replacement case.
IN_PROC_BROWSER_TEST_F(OmniboxFocusInteractiveTest,
                       NtpReplacementExtension_LocationReplacement) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Open the new tab, focus should be on the location bar.
  OpenNewTab();
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));

  // Install an extension that
  // 1) provides a replacement for chrome://newtab URL
  // 2) navigates away from the replacement
  WriteExtensionFile(FILE_PATH_LITERAL("ext_ntp.html"),
                     "<script src='ext_ntp.js'></script>");
  GURL final_ntp_url = embedded_test_server()->GetURL("/title1.html");
  WriteExtensionFile(FILE_PATH_LITERAL("ext_ntp.js"),
                     content::JsReplace("location.replace($1)", final_ntp_url));
  const Extension* extension = CreateAndLoadNtpReplacementExtension();
  ASSERT_TRUE(extension);

  // Open the new tab, because of the NTP extension behavior, the focus should
  // move to the tab contents.
  OpenNewTab();
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(final_ntp_url, web_contents->GetLastCommittedURL());
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));
  EXPECT_EQ(1, web_contents->GetController().GetEntryCount());
}

// Verify that pushState in an NTP-replacement extension results in the omnibox
// staying focused.  The focus should move to tab contents only when navigating
// away from the NTP - pushState doesn't navigate anywhere (i.e. it only changes
// the already existing navigation/history entry).
IN_PROC_BROWSER_TEST_F(OmniboxFocusInteractiveTest,
                       NtpReplacementExtension_PushState) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Open the new tab, focus should be on the location bar.
  OpenNewTab();
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));

  // Install an extension that provides a replacement for chrome://newtab URL.
  WriteExtensionFile(FILE_PATH_LITERAL("ext_ntp.html"),
                     "<body>NTP replacement extension</body>");
  const Extension* extension = CreateAndLoadNtpReplacementExtension();
  ASSERT_TRUE(extension);

  // Open the new tab.
  OpenNewTab();

  // Verify that ext_ntp.html is loaded in place of the NTP and that the omnibox
  // is focused.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ("NTP replacement extension",
            content::EvalJs(web_contents, "document.body.innerText"));
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));

  // pushState
  content::TestFrameNavigationObserver nav_observer(
      web_contents->GetPrimaryMainFrame());
  content::ExecuteScriptAsync(web_contents,
                              "history.pushState({}, '', '/push-state')");
  nav_observer.Wait();
  EXPECT_EQ(2, web_contents->GetController().GetEntryCount());
  EXPECT_EQ(extension->GetResourceURL("/push-state"),
            web_contents->GetController().GetLastCommittedEntry()->GetURL());

  // Verify that pushState didn't make the focus move away from the omnibox.
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));
}

// Verify that location.reload in an NTP-replacement extension results in the
// omnibox staying focused.  The focus should move to tab contents only when
// navigating away from the NTP - reload doesn't navigate away from the NTP.
IN_PROC_BROWSER_TEST_F(OmniboxFocusInteractiveTest,
                       NtpReplacementExtension_Reload) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Open the new tab, focus should be on the location bar.
  OpenNewTab();
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));

  // Install an extension that provides a replacement for chrome://newtab URL.
  WriteExtensionFile(FILE_PATH_LITERAL("ext_ntp.html"),
                     "<body>NTP replacement extension</body>");
  const Extension* extension = CreateAndLoadNtpReplacementExtension();
  ASSERT_TRUE(extension);

  // Open the new tab.
  OpenNewTab();

  // Verify that ext_ntp.html is loaded in place of the NTP and that the omnibox
  // is focused.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ("NTP replacement extension",
            content::EvalJs(web_contents, "document.body.innerText"));
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));

  // Execute `location.reload()`.
  content::TestFrameNavigationObserver nav_observer(
      web_contents->GetPrimaryMainFrame());
  content::ExecuteScriptAsync(web_contents, "window.location.reload()");
  nav_observer.Wait();
  EXPECT_EQ(1, web_contents->GetController().GetEntryCount());
  EXPECT_EQ("NTP replacement extension",
            content::EvalJs(web_contents, "document.body.innerText"));

  // Verify that `reload` didn't make the focus move away from the omnibox.
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));
}

// Verify that non-NTP extension->web navigations do NOT steal focus from the
// omnibox.
IN_PROC_BROWSER_TEST_F(OmniboxFocusInteractiveTest, OmniboxFocusStealing) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Install a test extension.
  TestExtensionDir dir;
  const char kManifest[] = R"(
      {
        "manifest_version": 2,
        "name": "Omnibox focus-testing extension",
        "version": "1.0"
      } )";
  dir.WriteManifest(kManifest);
  dir.WriteFile(FILE_PATH_LITERAL("ext.html"), "<p>Blah<p>");
  const Extension* extension = LoadExtension(dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // Navigate to an extension resource.
  GURL ext_url = extension->GetResourceURL("ext.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), ext_url));

  // Focus the location bar / omnibox.
  chrome::FocusLocationBar(browser());
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));

  // Trigger a renderer-initiated navigation from an extension resource to a web
  // page.  In the past such navigation might have resulted in
  // ShouldFork/OpenURL code path and might have stolen the focus from the
  // location bar / omnibox.
  GURL web_url = embedded_test_server()->GetURL("/title1.html");
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TestFrameNavigationObserver nav_observer(
      web_contents->GetPrimaryMainFrame());
  ASSERT_TRUE(content::ExecJs(
      web_contents, content::JsReplace("window.location = $1", web_url)));
  nav_observer.Wait();
  EXPECT_EQ(web_url, web_contents->GetLastCommittedURL());

  // Verify that the omnibox retained its focus.
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));
}

// Tab focus should not be stolen by the omnibox - https://crbug.com/1127220.
IN_PROC_BROWSER_TEST_F(OmniboxFocusInteractiveTest, TabFocusStealingFromOopif) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // CSP of the NTP page enforces that only HTTPS subframes may be used.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.AddDefaultHandlers(GetChromeTestDataDir());
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
  ASSERT_TRUE(https_server.Start());

  // Open the new tab, focus should be on the location bar.
  OpenNewTab();
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));

  // Focus the tab contents.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  web_contents->Focus();
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));

  // Inject a cross-site subframe into the NTP (simulating opening a
  // menu of Google applications from the NTP).
  const char kFrameInjectionScriptTemplate[] = R"(
      f = document.createElement('iframe');
      new Promise(resolve => {
        f.onload = function() {
            resolve("Frame injected successfully");
        }
        f.src = $1;
        document.body.appendChild(f);
      });
  )";
  GURL subframe_url = https_server.GetURL("/title1.html");
  // The NTP might be in the process of navigating or adding its other
  // subframes - this is why the test doesn't use TestNavigationObserver, but
  // instead waits for the frame's onload event.
  ASSERT_EQ("Frame injected successfully",
            content::EvalJs(web_contents,
                            content::JsReplace(kFrameInjectionScriptTemplate,
                                               subframe_url)));
  const auto frames =
      CollectAllRenderFrameHosts(web_contents->GetPrimaryPage());
  const auto it = base::ranges::find(
      frames, subframe_url, &content::RenderFrameHost::GetLastCommittedURL);
  ASSERT_NE(it, frames.cend());
  content::RenderFrameHost* subframe = *it;

  // Verify that the subframe has a different scheme and a different process
  // from the main frame.  This ensures that in the next step the navigation
  // will not be triggered by the regular BeginNavigation path, but instead
  // will go through content::RenderFrameProxyHost::OpenURL.
  content::RenderFrameHost* main_frame = web_contents->GetPrimaryMainFrame();
  EXPECT_NE(subframe->GetLastCommittedURL().scheme(),
            main_frame->GetLastCommittedURL().scheme());
  EXPECT_NE(subframe->GetProcess()->GetID(), main_frame->GetProcess()->GetID());

  // Trigger a subframe-initiated navigation of the main frame.
  const char kLinkClickingScriptTemplate[] = R"(
      a = document.createElement('a');
      a.href = $1;
      a.innerText = 'test link';
      a.target = '_top';
      document.body.appendChild(a)
      a.click();
  )";
  GURL target_url = embedded_test_server()->GetURL("/title2.html");
  {
    content::TestFrameNavigationObserver nav_observer(
        web_contents->GetPrimaryMainFrame());
    ASSERT_TRUE(content::ExecJs(
        subframe, content::JsReplace(kLinkClickingScriptTemplate, target_url)));
    nav_observer.Wait();
  }
  EXPECT_EQ(target_url, web_contents->GetLastCommittedURL());

  // Verify that during the navigation, the tab contents stayed focused.
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));

  // Secondary verification: Focus should move to the Omnibox after pressing the
  // Home button.
  {
    content::TestFrameNavigationObserver nav_observer(
        web_contents->GetPrimaryMainFrame());
    chrome::Home(browser(), WindowOpenDisposition::CURRENT_TAB);
    nav_observer.Wait();
  }
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));
}

// Tab focus should not be stolen by the omnibox - https://crbug.com/1127220.
IN_PROC_BROWSER_TEST_F(OmniboxFocusInteractiveTest,
                       TabFocusStealingFromMainFrame) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Open the new tab, focus should be on the location bar.
  OpenNewTab();
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));

  // Focus the tab contents.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  web_contents->Focus();
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));

  // Trigger a renderer-initiated navigation of the main frame.
  const char kLinkClickingScriptTemplate[] = R"(
      a = document.createElement('a');
      a.href = $1;
      a.innerText = 'test link';
      document.body.appendChild(a)
      a.click();
  )";
  GURL target_url = embedded_test_server()->GetURL("/title2.html");
  {
    content::TestFrameNavigationObserver nav_observer(
        web_contents->GetPrimaryMainFrame());
    ASSERT_TRUE(content::ExecJs(
        web_contents,
        content::JsReplace(kLinkClickingScriptTemplate, target_url)));
    nav_observer.Wait();
  }
  EXPECT_EQ(target_url, web_contents->GetLastCommittedURL());

  // Verify that during the navigation, the tab contents stayed focused.
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));

  // Secondary verification: Focus should move to the Omnibox after pressing the
  // Home button.
  {
    content::TestFrameNavigationObserver nav_observer(
        web_contents->GetPrimaryMainFrame());
    chrome::Home(browser(), WindowOpenDisposition::CURRENT_TAB);
    nav_observer.Wait();
  }
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));
}

class OmniboxFocusInteractiveFencedFrameTest
    : public OmniboxFocusInteractiveTest {
 public:
  OmniboxFocusInteractiveFencedFrameTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{blink::features::kFencedFrames, {}},
         {blink::features::kFencedFramesAPIChanges, {}},
         {features::kPrivacySandboxAdsAPIsOverride, {}},
         {blink::features::kFencedFramesDefaultMode, {}}},
        {/* disabled_features */});
  }
  ~OmniboxFocusInteractiveFencedFrameTest() override = default;

  void SetUpOnMainThread() override {
    OmniboxFocusInteractiveTest::SetUpOnMainThread();
    ASSERT_TRUE(https_server_.Start());
  }

 protected:
  net::EmbeddedTestServer& https_server() { return https_server_; }

 private:
  base::test::ScopedFeatureList feature_list_;
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

IN_PROC_BROWSER_TEST_F(OmniboxFocusInteractiveFencedFrameTest,
                       NtpReplacementExtension_LoadFencedFrame) {
  // Open the new tab, focus should be on the location bar.
  OpenNewTab();

  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));

  // Focus the tab contents.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  web_contents->Focus();
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));

  // FencedFrameTestHelper uses eval() function that is blocked by the
  // document's CSP on this page. So need to maually create a fenced frame for
  // avoiding the CSP policy.
  constexpr char kAddFencedFrameScript[] = R"({
      const fenced_frame = document.createElement('fencedframe');
      fenced_frame.config = new FencedFrameConfig($1);
      document.body.appendChild(fenced_frame);
  })";

  // Create a fenced frame and load a URL.
  // The fenced frame navigation should not affect the view focus.
  GURL fenced_frame_url = https_server().GetURL("/fenced_frames/title1.html");
  content::TestNavigationManager navigation(web_contents, fenced_frame_url);
  EXPECT_TRUE(content::ExecJs(
      web_contents->GetPrimaryMainFrame(),
      content::JsReplace(kAddFencedFrameScript, fenced_frame_url)));
  ASSERT_TRUE(navigation.WaitForNavigationFinished());

  // Verify that after the fenced frame navigation, the tab contents stayed
  // focused.
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));
}

}  // namespace extensions
