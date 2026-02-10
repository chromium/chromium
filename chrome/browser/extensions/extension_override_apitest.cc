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
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/extension_web_ui.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/extension_creator.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/constants.h"
#include "extensions/common/manifest_handlers/chrome_url_overrides_handler.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

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
    const base::DictValue& overrides =
        profile()->GetPrefs()->GetDict(ExtensionWebUI::kExtensionURLOverrides);

    const base::ListValue* values = overrides.FindList("history");
    if (!values)
      return false;

    std::set<std::string> seen_overrides;
    for (const auto& val : *values) {
      if (!val.is_dict()) {
        return false;
      }
      const base::DictValue& dict = val.GetDict();
      const std::string* entry = dict.FindString("entry");
      if (!entry || !seen_overrides.insert(*entry).second) {
        return false;
      }
    }

    return true;
  }

  // Returns AssertionSuccess() if the given |web_contents| is being actively
  // controlled by the extension with |extension_id|.
  testing::AssertionResult ExtensionControlsPage(
      content::WebContents* web_contents,
      const std::string& extension_id) {
    if (!web_contents->GetController().GetLastCommittedEntry()) {
      return testing::AssertionFailure() << "No last committed entry.";
    }
    // We can't just use WebContents::GetLastCommittedURL() here because
    // trickiness makes the WebContents think that it committed chrome://newtab
    // when dealing with the new tab page.
    GURL gurl = web_contents->GetController().GetLastCommittedEntry()->GetURL();
    if (!gurl.SchemeIs(kExtensionScheme)) {
      return testing::AssertionFailure() << gurl;
    }
    if (gurl.host() != extension_id) {
      return testing::AssertionFailure() << gurl;
    }
    return testing::AssertionSuccess();
  }

  // Returns AssertionSuccess() if the given |web_contents| is not being
  // actively controlled by any extension.
  testing::AssertionResult ExtensionDoesNotControlPage(
      content::WebContents* web_contents) {
    if (!web_contents->GetController().GetLastCommittedEntry()) {
      return testing::AssertionFailure() << "No last committed entry.";
    }
    // We can't just use WebContents::GetLastCommittedURL() here because
    // trickiness makes the WebContents think that it committed chrome://newtab
    // when dealing with the new tab page.
    GURL gurl = web_contents->GetController().GetLastCommittedEntry()->GetURL();
    if (gurl.SchemeIs(kExtensionScheme)) {
      return testing::AssertionFailure() << gurl;
    }
    return testing::AssertionSuccess();
  }

  base::FilePath data_dir() {
    return test_data_dir_.AppendASCII("override");
  }

  // Enables the extension with the given ID in incognito, waits for it to
  // reload and returns it.
  scoped_refptr<const Extension> EnableExtensionInIncognito(
      const ExtensionId& extension_id) {
    // Allowing in incognito requires a reload of the extension, so we have to
    // wait for it.
    TestExtensionRegistryObserver observer(ExtensionRegistry::Get(profile()),
                                           extension_id);
    util::SetIsIncognitoEnabled(extension_id, profile(), true);
    scoped_refptr<const Extension> extension =
        observer.WaitForExtensionLoaded();
    EXPECT_TRUE(extension);
    return extension;
  }
};

// Test for overriding the new tab page with an extension with "incognito":
// "spanning" (default if the "incognito" manifest key is unspecified).
IN_PROC_BROWSER_TEST_F(ExtensionOverrideTest, OverrideNewTab) {
  scoped_refptr<const Extension> extension =
      LoadExtension(data_dir().AppendASCII("newtab"));
  {
    // Navigate to the new tab page.  The overridden new tab page
    // will call chrome.test.sendMessage('controlled by first').
    ExtensionTestMessageListener listener;
    auto* web_contents = GetActiveWebContents();
    ASSERT_TRUE(NavigateToURL(web_contents, GURL("chrome://newtab/")));
    EXPECT_TRUE(ExtensionControlsPage(web_contents, extension->id()));
    EXPECT_TRUE(listener.WaitUntilSatisfied());
    EXPECT_EQ("controlled by first", listener.message());
  }
  {
    // Navigate an incognito tab to the new tab page, first without enabling the
    // extension in incognito. We should get the default new tab page.
    auto* incognito_web_contents =
        PlatformOpenURLOffTheRecord(profile(), GURL("chrome://newtab/"));
    EXPECT_TRUE(ExtensionDoesNotControlPage(incognito_web_contents));

    // Now enable the extension in incognito mode.
    extension = EnableExtensionInIncognito(extension->id());

    // Even after enabling in incognito, the extension still shouldn't override
    // the new tab page.
    ASSERT_TRUE(
        NavigateToURL(incognito_web_contents, GURL("chrome://newtab/")));
    EXPECT_TRUE(ExtensionDoesNotControlPage(incognito_web_contents));
  }
}

// Test for overriding the new tab page with an "incognito": "split" extension.
IN_PROC_BROWSER_TEST_F(ExtensionOverrideTest, OverrideNewTabSplitMode) {
  scoped_refptr<const Extension> extension =
      LoadExtension(data_dir().AppendASCII("newtab_split_mode"));
  {
    // Navigate to the new tab page.  The overridden new tab page
    // will call chrome.test.notifyPass().
    ResultCatcher catcher;
    auto* web_contents = GetActiveWebContents();
    ASSERT_TRUE(NavigateToURL(web_contents, GURL("chrome://newtab/")));
    EXPECT_TRUE(ExtensionControlsPage(web_contents, extension->id()));
    ASSERT_TRUE(catcher.GetNextResult());
  }
  {
    // Navigate an incognito tab to the new tab page, first without enabling the
    // extension in incognito. We should get the default new tab page.
    auto* incognito_web_contents =
        PlatformOpenURLOffTheRecord(profile(), GURL("chrome://newtab/"));
    EXPECT_TRUE(ExtensionDoesNotControlPage(incognito_web_contents));

    // Now enable the extension in incognito mode.
    extension = EnableExtensionInIncognito(extension->id());

    // Even after enabling in incognito the extension should still not be able
    // to override the new tab page. Normally "incognito": "split" extensions
    // can override incognito chrome pages if they are enabled in incognito, but
    // we never allow the new tab page to be overridden in incognito since we
    // need to ensure users see details about what incognito is (and isn't).
    ASSERT_TRUE(
        NavigateToURL(incognito_web_contents, GURL("chrome://newtab/")));
    EXPECT_TRUE(ExtensionDoesNotControlPage(incognito_web_contents));
  }
}

// Test for overriding the bookmarks page with an extension with "incognito":
// "spanning" (default if "incognito" is unspecified).
IN_PROC_BROWSER_TEST_F(ExtensionOverrideTest, OverrideBookmarks) {
  scoped_refptr<const Extension> extension =
      LoadExtension(data_dir().AppendASCII("bookmarks"));
  {
    // Navigate to the bookmarks page. The overridden page will call
    // chrome.test.notifyPass().
    ResultCatcher catcher;
    auto* web_contents = GetActiveWebContents();
    ASSERT_TRUE(NavigateToURL(web_contents, GURL("chrome://bookmarks/")));
    EXPECT_TRUE(ExtensionControlsPage(web_contents, extension->id()));
    ASSERT_TRUE(catcher.GetNextResult());
  }
  {
    // Navigate an incognito tab to the bookmarks, first without enabling the
    // extension in incognito. We should get the default bookmarks page.
    auto* incognito_web_contents =
        PlatformOpenURLOffTheRecord(profile(), GURL("chrome://bookmarks/"));
    EXPECT_TRUE(ExtensionDoesNotControlPage(incognito_web_contents));

    // Now enable the extension in incognito mode.
    extension = EnableExtensionInIncognito(extension->id());

    // Even after enabling in incognito, the extension still shouldn't override
    // the bookmarks page, as only "incognito": "split" extensions can override
    // incognito chrome pages.
#if BUILDFLAG(IS_ANDROID)
    // This is a bit strange, but we actually expect this NavigateToURL call to
    // fail on Android for the bookmarks page if it is not being overridden by
    // an extension. Instead it is swapped out with a Android NativePage, so the
    // web contents doesn't finish the navigation like NavigateToUrl expects.
    ASSERT_FALSE(
        NavigateToURL(incognito_web_contents, GURL("chrome://bookmarks/")));
#else
    ASSERT_TRUE(
        NavigateToURL(incognito_web_contents, GURL("chrome://bookmarks/")));
#endif  // BUILDFLAG(IS_ANDROID)
    EXPECT_TRUE(ExtensionDoesNotControlPage(incognito_web_contents));
  }
}

// Test for overriding the Bookmarks page with an "incognito": "split"
// extension.
IN_PROC_BROWSER_TEST_F(ExtensionOverrideTest, OverrideBookmarksSplitMode) {
  scoped_refptr<const Extension> extension =
      LoadExtension(data_dir().AppendASCII("bookmarks_split_mode"));
  {
    // Navigate to the bookmarks page. The overridden page will call
    // chrome.test.notifyPass().
    ResultCatcher catcher;
    auto* web_contents = GetActiveWebContents();
    ASSERT_TRUE(NavigateToURL(web_contents, GURL("chrome://bookmarks/")));
    EXPECT_TRUE(ExtensionControlsPage(web_contents, extension->id()));
    ASSERT_TRUE(catcher.GetNextResult());
  }
  {
    // Navigate an incognito tab to the bookmarks page, first without enabling
    // the extension in incognito. We should get the default bookmarks page.
    auto* incognito_web_contents =
        PlatformOpenURLOffTheRecord(profile(), GURL("chrome://bookmarks/"));
    EXPECT_TRUE(ExtensionDoesNotControlPage(incognito_web_contents));

    // Now enable the extension in incognito mode.
    extension = EnableExtensionInIncognito(extension->id());

    // After enabling in incognito the extension will be able to override the
    // bookmarks page. The overridden page will call chrome.test.notifyPass().
    ResultCatcher catcher;
    ASSERT_TRUE(
        NavigateToURL(incognito_web_contents, GURL("chrome://bookmarks/")));
    EXPECT_TRUE(ExtensionControlsPage(incognito_web_contents, extension->id()));
    ASSERT_TRUE(catcher.GetNextResult());
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

  auto* web_contents = GetActiveWebContents();

  {
    // Navigate to the new tab page. Last extension installed wins, so
    // the new tab page should be controlled by the second extension.
    ExtensionTestMessageListener listener;
    ASSERT_TRUE(NavigateToURL(web_contents, GURL("chrome://newtab/")));
    EXPECT_TRUE(ExtensionControlsPage(web_contents, extension2_id));
    EXPECT_TRUE(listener.WaitUntilSatisfied());
    EXPECT_EQ("controlled by second", listener.message());
  }

  // Unload and reload the first extension. This should *not* result in the
  // first extension moving to the front of the line.
  ReloadExtension(extension1_id);

  {
    // The page should still be controlled by the second extension.
    ExtensionTestMessageListener listener;
    ASSERT_TRUE(NavigateToURL(web_contents, GURL("chrome://newtab/")));
    EXPECT_TRUE(ExtensionControlsPage(web_contents, extension2_id));
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
    ASSERT_TRUE(NavigateToURL(web_contents, GURL("chrome://newtab/")));
    EXPECT_TRUE(ExtensionControlsPage(web_contents, extension2_id));
    EXPECT_TRUE(listener.WaitUntilSatisfied());
    EXPECT_EQ("controlled by second", listener.message());
  }

  // Unload the (controlling) second extension. Now, and only now, should
  // extension1 take over.
  UnloadExtension(extension2_id);

  {
    ExtensionTestMessageListener listener;
    ASSERT_TRUE(NavigateToURL(web_contents, GURL("chrome://newtab/")));
    EXPECT_TRUE(ExtensionControlsPage(web_contents, extension1_id));
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
  auto* web_contents = GetActiveWebContents();
  {
    // Navigate to the new tab page. Last extension installed wins, so
    // the new tab page should be controlled by the second extension.
    ExtensionTestMessageListener listener;
    ASSERT_TRUE(NavigateToURL(web_contents, GURL("chrome://newtab/")));
    EXPECT_TRUE(ExtensionControlsPage(web_contents, extension2_id));
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
    EXPECT_TRUE(ExtensionControlsPage(web_contents, extension1_id));
  }

  UnloadExtension(extension1_id);
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));
  EXPECT_FALSE(ExtensionControlsPage(web_contents, extension1_id));
}

// Check that when an overridden new tab page has focus, a subframe navigation
// on that page does not steal the focus away by focusing the omnibox.
// See https://crbug.com/41306576.
// TODO(crbug.com/40804036): Flaky on Linux.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_SubframeNavigationInOverridenNTPDoesNotAffectFocus \
  DISABLED_SubframeNavigationInOverridenNTPDoesNotAffectFocus
#else
#define MAYBE_SubframeNavigationInOverridenNTPDoesNotAffectFocus \
  SubframeNavigationInOverridenNTPDoesNotAffectFocus
#endif  // BUILDFLAG(IS_LINUX)
IN_PROC_BROWSER_TEST_F(
    ExtensionOverrideTest,
    MAYBE_SubframeNavigationInOverridenNTPDoesNotAffectFocus) {
  // Load an extension that overrides the new tab page.
  const Extension* extension = LoadExtension(data_dir().AppendASCII("newtab"));

  // Navigate to the new tab page.  The overridden new tab page
  // will call chrome.test.sendMessage('controlled by first').
  ExtensionTestMessageListener listener("controlled by first");
  auto* contents = GetActiveWebContents();
  ASSERT_TRUE(NavigateToURL(contents, GURL(chrome::kChromeUINewTabURL)));
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

IN_PROC_BROWSER_TEST_F(ExtensionOverrideTest, OverrideHistory) {
  ASSERT_TRUE(RunExtensionTest("override/history")) << message_;
  {
    ResultCatcher catcher;
    // Navigate to the history page.  The overridden history page
    // will call chrome.test.notifyPass() .
    auto* web_contents = GetActiveWebContents();
    ASSERT_TRUE(NavigateToURL(web_contents, GURL("chrome://history/")));
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
        profile(), URLOverrides::GetChromeURLOverrides(extension));
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
  base::ListValue list;
  for (size_t i = 0; i < 3; ++i) {
    base::DictValue dict;
    dict.Set("entry", "http://www.google.com/");
    dict.Set("active", true);
    list.Append(std::move(dict));
  }

  {
    ScopedDictPrefUpdate update(profile()->GetPrefs(),
                                ExtensionWebUI::kExtensionURLOverrides);
    update->Set("history", std::move(list));
  }

  ASSERT_FALSE(CheckHistoryOverridesContainsNoDupes());

  ExtensionWebUI::InitializeChromeURLOverrides(profile());

  ASSERT_TRUE(CheckHistoryOverridesContainsNoDupes());
}

}  // namespace extensions
