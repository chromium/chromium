// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_web_ui.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/extensions/chrome_manifest_url_handlers.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/extension_creator.h"
#include "extensions/common/constants.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

using content::WebContents;

namespace extensions {

class ExtensionOverrideTest : public ExtensionApiTest {
 protected:
  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  bool CheckHistoryOverridesContainsNoDupes() {
    // There should be no duplicate entries in the preferences.
    const base::Value::Dict& overrides =
        browser()->profile()->GetPrefs()->GetDict(
            ExtensionWebUI::kExtensionURLOverrides);

    const base::Value::List* values = overrides.FindList("history");
    if (!values)
      return false;

    std::set<std::string> seen_overrides;
    for (const auto& val : *values) {
      if (!val.is_dict()) {
        return false;
      }
      const base::Value::Dict& dict = val.GetDict();
      const std::string* entry = dict.FindString("entry");
      if (!entry || seen_overrides.count(*entry) != 0)
        return false;
      seen_overrides.insert(*entry);
    }

    return true;
  }

  // Returns AssertionSuccess() if the given |web_contents| is being actively
  // controlled by the extension with |extension_id|.
  testing::AssertionResult ExtensionControlsPage(
      content::WebContents* web_contents,
      const std::string& extension_id) {
    if (!web_contents->GetController().GetLastCommittedEntry())
      return testing::AssertionFailure() << "No last committed entry.";
    // We can't just use WebContents::GetLastCommittedURL() here because
    // trickiness makes it think that it committed chrome://newtab.
    GURL gurl = web_contents->GetController().GetLastCommittedEntry()->GetURL();
    if (!gurl.SchemeIs(kExtensionScheme))
      return testing::AssertionFailure() << gurl;
    if (gurl.host_piece() != extension_id)
      return testing::AssertionFailure() << gurl;
    return testing::AssertionSuccess();
  }

  base::FilePath data_dir() {
    return test_data_dir_.AppendASCII("override");
  }
};

// Basic test for overriding the NTP.
IN_PROC_BROWSER_TEST_F(ExtensionOverrideTest, OverrideNewTab) {
  const Extension* extension = LoadExtension(data_dir().AppendASCII("newtab"));
  {
    // Navigate to the new tab page.  The overridden new tab page
    // will call chrome.test.sendMessage('controlled by first').
    ExtensionTestMessageListener listener;
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL("chrome://newtab/")));
    EXPECT_TRUE(ExtensionControlsPage(
        browser()->tab_strip_model()->GetActiveWebContents(),
        extension->id()));
    EXPECT_TRUE(listener.WaitUntilSatisfied());
    EXPECT_EQ("controlled by first", listener.message());
  }
}

// Check having multiple extensions with the same override.
IN_PROC_BROWSER_TEST_F(ExtensionOverrideTest, OverrideNewTabMultiple) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath temp_path = temp_dir.GetPath();
  base::FilePath extension1_path = temp_path.AppendASCII("extension1.crx");
  base::FilePath extension1_pem_path = temp_path.AppendASCII("extension1.pem");
  base::FilePath extension1_unpacked_path = data_dir().AppendASCII("newtab");
  ASSERT_TRUE(ExtensionCreator().Run(extension1_unpacked_path, extension1_path,
                                     base::FilePath(), extension1_pem_path, 0));

  base::FilePath extension2_path = temp_path.AppendASCII("extension2.crx");
  base::FilePath extension2_pem_path = temp_path.AppendASCII("extension2.pem");
  base::FilePath extension2_unpacked_path = data_dir().AppendASCII("newtab2");
  ASSERT_TRUE(ExtensionCreator().Run(extension2_unpacked_path, extension2_path,
                                     base::FilePath(), extension2_pem_path, 0));

  // Prefer IDs because loading/unloading invalidates the extension ptrs.
  const std::string extension1_id =
      InstallExtensionWithPermissionsGranted(extension1_path, 1)->id();
  const std::string extension2_id =
      InstallExtensionWithPermissionsGranted(extension2_path, 1)->id();

  {
    // Navigate to the new tab page. Last extension installed wins, so
    // the new tab page should be controlled by the second extension.
    ExtensionTestMessageListener listener;
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL("chrome://newtab/")));
    EXPECT_TRUE(ExtensionControlsPage(
        browser()->tab_strip_model()->GetActiveWebContents(),
        extension2_id));
    EXPECT_TRUE(listener.WaitUntilSatisfied());
    EXPECT_EQ("controlled by second", listener.message());
  }

  // Unload and reload the first extension. This should *not* result in the
  // first extension moving to the front of the line.
  ReloadExtension(extension1_id);

  {
    // The page should still be controlled by the second extension.
    ExtensionTestMessageListener listener;
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL("chrome://newtab/")));
    EXPECT_TRUE(ExtensionControlsPage(
        browser()->tab_strip_model()->GetActiveWebContents(),
        extension2_id));
    EXPECT_TRUE(listener.WaitUntilSatisfied());
    EXPECT_EQ("controlled by second", listener.message());
  }

  {
    // Upgrade the first extension to a version that uses a different NTP url.
    // This should *not* result in the first extension moving to the front of
    // the line.
    base::FilePath update_path = temp_path.AppendASCII("extension1_update.crx");
    base::FilePath update_pem_path = extension1_pem_path;
    base::FilePath update_unpacked_path =
        data_dir().AppendASCII("newtab_upgrade");
    ASSERT_TRUE(ExtensionCreator().Run(update_unpacked_path, update_path,
                                       update_pem_path, base::FilePath(), 0));

    const Extension* updated_extension =
        UpdateExtension(extension1_id, update_path, 0);
    ASSERT_TRUE(updated_extension);
    EXPECT_EQ(extension1_id, updated_extension->id());
  }

  {
    // The page should still be controlled by the second extension.
    ExtensionTestMessageListener listener;
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL("chrome://newtab/")));
    EXPECT_TRUE(ExtensionControlsPage(
        browser()->tab_strip_model()->GetActiveWebContents(), extension2_id));
    EXPECT_TRUE(listener.WaitUntilSatisfied());
    EXPECT_EQ("controlled by second", listener.message());
  }

  // Unload the (controlling) second extension. Now, and only now, should
  // extension1 take over.
  UnloadExtension(extension2_id);

  {
    ExtensionTestMessageListener listener;
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL("chrome://newtab/")));
    EXPECT_TRUE(ExtensionControlsPage(
        browser()->tab_strip_model()->GetActiveWebContents(),
        extension1_id));
    EXPECT_TRUE(listener.WaitUntilSatisfied());
    EXPECT_EQ("controlled by first upgrade", listener.message());
  }
}

// Test that unloading an extension overriding the page reloads the page with
// the proper url.
IN_PROC_BROWSER_TEST_F(ExtensionOverrideTest,
                       OverridingExtensionUnloadedWithPageOpen) {
  // Prefer IDs because loading/unloading invalidates the extension ptrs.
  const std::string extension1_id =
      LoadExtension(data_dir().AppendASCII("newtab"))->id();
  const std::string extension2_id =
      LoadExtension(data_dir().AppendASCII("newtab2"))->id();
  {
    // Navigate to the new tab page. Last extension installed wins, so
    // the new tab page should be controlled by the second extension.
    ExtensionTestMessageListener listener;
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL("chrome://newtab/")));
    EXPECT_TRUE(ExtensionControlsPage(
        browser()->tab_strip_model()->GetActiveWebContents(),
        extension2_id));
    EXPECT_TRUE(listener.WaitUntilSatisfied());
    EXPECT_EQ("controlled by second", listener.message());
  }

  {
    // Unload the controlling extension. The page should be automatically
    // reloaded with the new controlling extension.
    ExtensionTestMessageListener listener;
    UnloadExtension(extension2_id);
    EXPECT_TRUE(listener.WaitUntilSatisfied());
    EXPECT_EQ("controlled by first", listener.message());
    EXPECT_TRUE(ExtensionControlsPage(
        browser()->tab_strip_model()->GetActiveWebContents(),
        extension1_id));
  }

  UnloadExtension(extension1_id);
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(active_tab));
  EXPECT_FALSE(ExtensionControlsPage(active_tab, extension1_id));
}

IN_PROC_BROWSER_TEST_F(ExtensionOverrideTest, OverrideNewTabIncognito) {
  LoadExtension(data_dir().AppendASCII("newtab"));

  // Navigate an incognito tab to the new tab page.  We should get the actual
  // new tab page because we can't load chrome-extension URLs in incognito.
  Browser* otr_browser =
      OpenURLOffTheRecord(browser()->profile(), GURL("chrome://newtab/"));
  WebContents* tab = otr_browser->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(tab->GetController().GetVisibleEntry());
  EXPECT_FALSE(tab->GetController().GetVisibleEntry()->GetURL().
               SchemeIs(kExtensionScheme));
}

// Check that when an overridden new tab page has focus, a subframe navigation
// on that page does not steal the focus away by focusing the omnibox.
// See https://crbug.com/700124.
// Flaky, http://crbug.com/1269169.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_SubframeNavigationInOverridenNTPDoesNotAffectFocus \
  DISABLED_SubframeNavigationInOverridenNTPDoesNotAffectFocus
#else
#define MAYBE_SubframeNavigationInOverridenNTPDoesNotAffectFocus \
  SubframeNavigationInOverridenNTPDoesNotAffectFocus
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
IN_PROC_BROWSER_TEST_F(
    ExtensionOverrideTest,
    MAYBE_SubframeNavigationInOverridenNTPDoesNotAffectFocus) {
  // Load an extension that overrides the new tab page.
  const Extension* extension = LoadExtension(data_dir().AppendASCII("newtab"));

  // Navigate to the new tab page.  The overridden new tab page
  // will call chrome.test.sendMessage('controlled by first').
  ExtensionTestMessageListener listener("controlled by first");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUINewTabURL)));
  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(ExtensionControlsPage(contents, extension->id()));
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  // Start off with the main page focused.
  contents->Focus();
  EXPECT_TRUE(contents->GetRenderWidgetHostView()->HasFocus());

  // Inject an iframe and navigate it to a cross-site URL.  With
  // --site-per-process, this will go into a separate process.
  GURL cross_site_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  std::string script = "var f = document.createElement('iframe');\n"
                       "f.src = '" + cross_site_url.spec() + "';\n"
                       "document.body.appendChild(f);\n";
  EXPECT_TRUE(ExecJs(contents, script));
  EXPECT_TRUE(WaitForLoadStop(contents));

  // The page should still have focus.  The cross-process subframe navigation
  // above should not try to focus the omnibox, which would make this false.
  EXPECT_TRUE(contents->GetRenderWidgetHostView()->HasFocus());
}

// Times out consistently on Win, http://crbug.com/45173.
#if BUILDFLAG(IS_WIN)
#define MAYBE_OverrideHistory DISABLED_OverrideHistory
#else
#define MAYBE_OverrideHistory OverrideHistory
#endif  // BUILDFLAG(IS_WIN)

IN_PROC_BROWSER_TEST_F(ExtensionOverrideTest, MAYBE_OverrideHistory) {
  ASSERT_TRUE(RunExtensionTest("override/history")) << message_;
  {
    ResultCatcher catcher;
    // Navigate to the history page.  The overridden history page
    // will call chrome.test.notifyPass() .
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL("chrome://history/")));
    ASSERT_TRUE(catcher.GetNextResult());
  }
}

// Regression test for http://crbug.com/41442.
IN_PROC_BROWSER_TEST_F(ExtensionOverrideTest, ShouldNotCreateDuplicateEntries) {
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("override/history"));
  ASSERT_TRUE(extension);

  // Simulate several LoadExtension() calls happening over the lifetime of
  // a preferences file without corresponding UnloadExtension() calls.
  for (size_t i = 0; i < 3; ++i) {
    ExtensionWebUI::RegisterOrActivateChromeURLOverrides(
        browser()->profile(),
        URLOverrides::GetChromeURLOverrides(extension));
  }

  ASSERT_TRUE(CheckHistoryOverridesContainsNoDupes());
}

// TODO(devlin): This test seems a bit contrived. How would we end up with
// duplicate entries created?
IN_PROC_BROWSER_TEST_F(ExtensionOverrideTest, ShouldCleanUpDuplicateEntries) {
  // Simulate several LoadExtension() calls happening over the lifetime of
  // a preferences file without corresponding UnloadExtension() calls. This is
  // the same as the above test, except for that it is testing the case where
  // the file already contains dupes when an extension is loaded.
  base::Value::List list;
  for (size_t i = 0; i < 3; ++i) {
    base::Value::Dict dict;
    dict.Set("entry", "http://www.google.com/");
    dict.Set("active", true);
    list.Append(std::move(dict));
  }

  {
    ScopedDictPrefUpdate update(browser()->profile()->GetPrefs(),
                                ExtensionWebUI::kExtensionURLOverrides);
    update->Set("history", std::move(list));
  }

  ASSERT_FALSE(CheckHistoryOverridesContainsNoDupes());

  ExtensionWebUI::InitializeChromeURLOverrides(profile());

  ASSERT_TRUE(CheckHistoryOverridesContainsNoDupes());
}

}  // namespace extensions
