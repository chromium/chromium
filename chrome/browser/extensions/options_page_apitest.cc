// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/functional/bind.h"
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

}  // namespace extensions
