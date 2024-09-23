// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_view_host_factory.h"

#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_view_host.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/mojom/view_type.mojom.h"

namespace extensions {

using ExtensionViewHostFactoryTest = ExtensionBrowserTest;

// Tests that ExtensionHosts are created with the correct type and profiles.
IN_PROC_BROWSER_TEST_F(ExtensionViewHostFactoryTest, CreateExtensionHosts) {
  // Load a very simple extension with just a background page.
  scoped_refptr<const Extension> extension =
      LoadExtension(test_data_dir_.AppendASCII("api_test")
                        .AppendASCII("browser_action")
                        .AppendASCII("none"));
  ASSERT_TRUE(extension.get());

  content::BrowserContext* browser_context = browser()->profile();

  // Popup hosts are created with the correct type and profile.
  std::unique_ptr<ExtensionViewHost> host =
      ExtensionViewHostFactory::CreatePopupHost(extension->url(), browser());
  EXPECT_EQ(extension.get(), host->extension());
  EXPECT_EQ(browser_context, host->browser_context());
  EXPECT_EQ(mojom::ViewType::kExtensionPopup, host->extension_host_type());
}

// Tests that side panel hosts are created with the correct profile and
// browsers.
IN_PROC_BROWSER_TEST_F(ExtensionViewHostFactoryTest,
                       CreateExtensionSidePanelHost) {
  // Load a very simple extension with just a background page.
  scoped_refptr<const Extension> extension =
      LoadExtension(test_data_dir_.AppendASCII("api_test")
                        .AppendASCII("side_panel")
                        .AppendASCII("simple_default"));
  ASSERT_TRUE(extension.get());

  content::BrowserContext* browser_context = browser()->profile();

  {
    // Create a side panel host with a browser passed in.
    std::unique_ptr<ExtensionViewHost> host =
        ExtensionViewHostFactory::CreateSidePanelHost(extension->url(),
                                                      browser(),
                                                      /*web_contents=*/nullptr);
    EXPECT_EQ(extension.get(), host->extension());
    EXPECT_EQ(browser_context, host->browser_context());
    EXPECT_EQ(browser(), host->GetBrowser());
    EXPECT_EQ(mojom::ViewType::kExtensionSidePanel,
              host->extension_host_type());
  }

  {
    // Create a side panel host with a tab based WebContents passed in.
    std::unique_ptr<ExtensionViewHost> host =
        ExtensionViewHostFactory::CreateSidePanelHost(
            extension->url(), /*browser=*/nullptr,
            browser()->tab_strip_model()->GetActiveWebContents());
    EXPECT_EQ(extension.get(), host->extension());
    EXPECT_EQ(browser_context, host->browser_context());
    EXPECT_EQ(browser(), host->GetBrowser());
    EXPECT_EQ(mojom::ViewType::kExtensionSidePanel,
              host->extension_host_type());
  }
}

}  // namespace extensions
