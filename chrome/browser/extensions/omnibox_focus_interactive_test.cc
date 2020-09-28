// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/ntp_overridden_bubble_delegate.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/test/test_extension_dir.h"

namespace extensions {

class OmniboxFocusInteractiveTest : public ExtensionBrowserTest {
 public:
  OmniboxFocusInteractiveTest() = default;
  ~OmniboxFocusInteractiveTest() override = default;

 protected:
  void WriteExtensionFile(const base::FilePath::StringType& filename,
                          base::StringPiece contents) {
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
    if (!extension)
      return nullptr;

    // Prevent a focus-stealing focus bubble that warns the user that "An
    // extension has changed what page is shown when you open a new tab."
    ExtensionPrefs* prefs = ExtensionPrefs::Get(browser()->profile());
    prefs->UpdateExtensionPref(
        extension->id(), NtpOverriddenBubbleDelegate::kNtpBubbleAcknowledged,
        std::make_unique<base::Value>(true));

    return extension;
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
  chrome::NewTab(browser());
  ASSERT_NO_FATAL_FAILURE(EXPECT_TRUE(content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents())));
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
  chrome::NewTab(browser());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NO_FATAL_FAILURE(EXPECT_TRUE(content::WaitForLoadStop(web_contents)));
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
    content::TestNavigationObserver nav_observer(web_contents, 1);
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
  chrome::NewTab(browser());
  ASSERT_NO_FATAL_FAILURE(EXPECT_TRUE(content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents())));
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));

  // Install an extension that provides a replacement for chrome://newtab URL.
  WriteExtensionFile(FILE_PATH_LITERAL("ext_ntp.html"),
                     "<body>NTP replacement extension</body>");
  const Extension* extension = CreateAndLoadNtpReplacementExtension();
  ASSERT_TRUE(extension);

  // Open the new tab.
  chrome::NewTab(browser());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NO_FATAL_FAILURE(EXPECT_TRUE(content::WaitForLoadStop(web_contents)));

  // Verify that ext_ntp.html is loaded in place of the NTP and that the omnibox
  // is focused.
  std::string document_body;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents, "domAutomationController.send(document.body.innerText)",
      &document_body));
  EXPECT_EQ("NTP replacement extension", document_body);
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
  content::TestNavigationObserver nav_observer(web_contents, 1);
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
  chrome::NewTab(browser());
  ASSERT_NO_FATAL_FAILURE(EXPECT_TRUE(content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents())));
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
  chrome::NewTab(browser());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NO_FATAL_FAILURE(EXPECT_TRUE(content::WaitForLoadStop(web_contents)));
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
  chrome::NewTab(browser());
  ASSERT_NO_FATAL_FAILURE(EXPECT_TRUE(content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents())));
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));

  // Install an extension that provides a replacement for chrome://newtab URL.
  WriteExtensionFile(FILE_PATH_LITERAL("ext_ntp.html"),
                     "<body>NTP replacement extension</body>");
  const Extension* extension = CreateAndLoadNtpReplacementExtension();
  ASSERT_TRUE(extension);

  // Open the new tab.
  chrome::NewTab(browser());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NO_FATAL_FAILURE(EXPECT_TRUE(content::WaitForLoadStop(web_contents)));

  // Verify that ext_ntp.html is loaded in place of the NTP and that the omnibox
  // is focused.
  std::string document_body;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents, "domAutomationController.send(document.body.innerText)",
      &document_body));
  EXPECT_EQ("NTP replacement extension", document_body);
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));

  // pushState
  content::TestNavigationObserver nav_observer(web_contents, 1);
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
  chrome::NewTab(browser());
  ASSERT_NO_FATAL_FAILURE(EXPECT_TRUE(content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents())));
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));

  // Install an extension that provides a replacement for chrome://newtab URL.
  WriteExtensionFile(FILE_PATH_LITERAL("ext_ntp.html"),
                     "<body>NTP replacement extension</body>");
  const Extension* extension = CreateAndLoadNtpReplacementExtension();
  ASSERT_TRUE(extension);

  // Open the new tab.
  chrome::NewTab(browser());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NO_FATAL_FAILURE(EXPECT_TRUE(content::WaitForLoadStop(web_contents)));

  // Verify that ext_ntp.html is loaded in place of the NTP and that the omnibox
  // is focused.
  std::string document_body;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents, "domAutomationController.send(document.body.innerText)",
      &document_body));
  EXPECT_EQ("NTP replacement extension", document_body);
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));

  // Execute `location.reload()`.
  content::TestNavigationObserver nav_observer(web_contents, 1);
  content::ExecuteScriptAsync(web_contents, "window.location.reload()");
  nav_observer.Wait();
  EXPECT_EQ(1, web_contents->GetController().GetEntryCount());
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents, "domAutomationController.send(document.body.innerText)",
      &document_body));
  EXPECT_EQ("NTP replacement extension", document_body);

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
  ui_test_utils::NavigateToURL(browser(), ext_url);

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
  content::TestNavigationObserver nav_observer(web_contents, 1);
  ASSERT_TRUE(content::ExecuteScript(
      web_contents, content::JsReplace("window.location = $1", web_url)));
  nav_observer.Wait();
  EXPECT_EQ(web_url, web_contents->GetLastCommittedURL());

  // Verify that the omnibox retained its focus.
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  EXPECT_FALSE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));
}

}  // namespace extensions
