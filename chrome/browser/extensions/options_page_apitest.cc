// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <string>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/tab_list/tab_list_interface_observer.h"
#include "chrome/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/test_extension_dir.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {
namespace {

// `kPreventExtensionResourceFetchAcrossIsolatedWorlds` is declared in
// `third_party/blink/renderer/platform/loader/fetch/resource.h`. Because
// `chrome/DEPS` forbids browser-side code from importing renderer-internal
// headers under `third_party/blink/renderer/` (only `third_party/blink/public/`
// is permitted), the feature is defined locally here so it can be enabled via
// `base::test::ScopedFeatureList` and propagated to renderer processes.
BASE_FEATURE(kPreventExtensionResourceFetchAcrossIsolatedWorlds,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Waits for a tab to be added to the tab list. Proceeds immediately if the tab
// was added before the call to Wait().
class TabAddedWaiter : public TabListInterfaceObserver {
 public:
  explicit TabAddedWaiter(TabListInterface* tab_list) : tab_list_(tab_list) {
    tab_list_->AddTabListInterfaceObserver(this);
  }

  ~TabAddedWaiter() override {
    tab_list_->RemoveTabListInterfaceObserver(this);
  }

  void Wait() {
    // If the tab was already added, there's nothing to do.
    if (tab_added_) {
      return;
    }
    run_loop_.Run();
  }

  // TabListInterfaceObserver:
  void OnTabAdded(TabListInterface& tab_list,
                  tabs::TabInterface* tab,
                  int index) override {
    tab_added_ = true;
    if (run_loop_.running()) {
      run_loop_.Quit();
    }
  }

 private:
  raw_ptr<TabListInterface> tab_list_;
  base::RunLoop run_loop_;
  bool tab_added_ = false;
};

}  // namespace

// Test that an extension with an options page makes an 'Options' button appear
// on chrome://extensions, and that clicking the button opens a new tab with the
// extension's options page.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, OptionsPage) {
  TestExtensionDir extension_dir;
  extension_dir.WriteFile(FILE_PATH_LITERAL("options.html"),
                          "<html><body><div>Options Here</div></body></html>");

  extension_dir.WriteManifest(base::DictValue()
                                  .Set("manifest_version", 2)
                                  .Set("name", "Options Test")
                                  .Set("options_page", "options.html")
                                  .Set("version", "1"));

  scoped_refptr<const Extension> extension =
      InstallExtension(extension_dir.Pack(), 1);
  ASSERT_TRUE(extension.get());

  // Go to the Extension Settings page and click the button.
  auto* web_contents = GetActiveWebContents();
  ASSERT_TRUE(NavigateToURL(web_contents,
                            GURL("chrome://extensions?id=" + extension->id())));
  ASSERT_TRUE(content::WaitForLoadStop(web_contents));
  auto* tab_list = TabListInterface::From(browser_window_interface());
  ASSERT_TRUE(tab_list);

  // The click will open a new tab.
  TabAddedWaiter tab_add(tab_list);

  // Used to simulate a click on the 'Extension options' link.
  // NOTE: This relies on the layout of the chrome://extensions page, and may
  // need to be updated if that layout changes.
  static constexpr char kScriptClickOptionButton[] = R"(
    (function() {
      var button = document.querySelector('extensions-manager').
                    shadowRoot.querySelector('extensions-detail-view').
                    shadowRoot.querySelector('#extensionsOptions');
      button.click();
    })();)";

  EXPECT_TRUE(content::ExecJs(web_contents, kScriptClickOptionButton));
  tab_add.Wait();

  ASSERT_EQ(2, tab_list->GetTabCount());
  content::WebContents* tab = tab_list->GetTab(1)->GetContents();
  ASSERT_TRUE(tab);
  EXPECT_TRUE(content::WaitForLoadStop(tab));
  EXPECT_EQ(extension->GetResourceURL("options.html"),
            tab->GetLastCommittedURL());
}

// Tests that navigating directly to chrome://extensions?options=<id> to an
// extension with an embedded options page loads that extension's options page.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest,
                       LoadChromeExtensionsWithOptionsParamWhenEmbedded) {
  TestExtensionDir extension_dir;
  extension_dir.WriteFile(FILE_PATH_LITERAL("options.html"),
                          "<script src=\"options.js\"></script>\n");
  extension_dir.WriteFile(
      FILE_PATH_LITERAL("options.js"),
      "chrome.tabs.getCurrent(function(tab) {\n"
      "  chrome.test.sendMessage(tab ? 'tab' : 'embedded');\n"
      "});\n");
  extension_dir.WriteManifest(
      base::DictValue()
          .Set("manifest_version", 2)
          .Set("name", "Extension for options param test")
          .Set("options_ui", base::DictValue().Set("page", "options.html"))
          .Set("version", "1"));

  ExtensionTestMessageListener listener;
  scoped_refptr<const Extension> extension =
      InstallExtension(extension_dir.Pack(), 1);
  ASSERT_TRUE(extension.get());
  ASSERT_TRUE(
      NavigateToURL(GetActiveWebContents(),
                    GURL("chrome://extensions?options=" + extension->id())));
  ASSERT_TRUE(listener.WaitUntilSatisfied());
  ASSERT_EQ("embedded", listener.message());
}

// Test fixture that enables the
// `kPreventExtensionResourceFetchAcrossIsolatedWorlds` feature for testing
// cross-world extension resource fetch behavior in extensions tests.
class ExtensionOptionsPreloadTest : public ExtensionBrowserTest {
 public:
  ExtensionOptionsPreloadTest() {
    // Feature overrides in browser tests must be initialized in the fixture
    // constructor rather than inside the test method because
    // `base::FeatureList` is sealed as immutable during browser startup before
    // `content::BrowserTestBase::ProxyRunTestOnMainThreadLoop()` executes, and
    // initializing here ensures the feature flag is passed to child renderer
    // processes.
    scoped_feature_list_.InitAndEnableFeature(
        kPreventExtensionResourceFetchAcrossIsolatedWorlds);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Verifies that preloading a script resource in an extension page (such as an
// options page) via `<link rel="modulepreload">` executes the script
// successfully and does not trigger a cross-world extension resource mismatch
// console warning when
// `kPreventExtensionResourceFetchAcrossIsolatedWorlds` is enabled.
IN_PROC_BROWSER_TEST_F(ExtensionOptionsPreloadTest,
                       ModulePreloadMismatchWarningInOptionsPage) {
  // Set up a test extension containing an options page with a `<link
  // rel="modulepreload">` element and a `<script type="module">` tag.
  TestExtensionDir test_dir;
  static constexpr char kManifest[] =
      R"({
           "manifest_version": 3,
           "name": "Cross-World Preload Mismatch Repro",
           "version": "1.0",
           "options_page": "options.html"
         })";
  static constexpr char kOptionsHtml[] =
      R"(<!DOCTYPE html>
         <html>
         <head>
             <title>Preload Test</title>
             <!-- Preloading an extension resource -->
             <link rel="modulepreload" href="script.js">
             <!-- Executing the script -->
             <script type="module" src="script.js"></script>
         </head>
         <body>
             <h1>Preload Test</h1>
         </body>
         </html>)";
  static constexpr char kScriptJs[] =
      R"(chrome.test.sendMessage("script executed");)";
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("options.html"), kOptionsHtml);
  test_dir.WriteFile(FILE_PATH_LITERAL("script.js"), kScriptJs);

  // Set up a message listener to wait for the script to execute.
  ExtensionTestMessageListener listener("script executed");

  // Load the unpacked extension into the browser.
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // Set up a console observer on the active web contents to monitor for any
  // cross-world extension resource mismatch warning.
  content::WebContents* web_contents = GetActiveWebContents();
  content::WebContentsConsoleObserver console_observer(web_contents);
  console_observer.SetPattern(
      "*is found, but is not used because it is a cross-world extension "
      "resource mismatch.*");

  // Navigate the browser to the extension's options page.
  ASSERT_TRUE(
      NavigateToURL(web_contents, extension->GetResourceURL("options.html")));

  // Verify that the script successfully executes.
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  // Verify that no cross-world mismatch warning was logged to the console.
  EXPECT_TRUE(console_observer.messages().empty());
}

}  // namespace extensions
