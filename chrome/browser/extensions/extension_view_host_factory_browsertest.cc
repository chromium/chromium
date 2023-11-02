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

typedef ExtensionBrowserTest ExtensionViewHostFactoryTest;

// Tests that ExtensionHosts are created with the correct type and profiles.
IN_PROC_BROWSER_TEST_F(ExtensionViewHostFactoryTest, CreateExtensionHosts) {
  // Load a very simple extension with just a background page.
  scoped_refptr<const Extension> extension =
      LoadExtension(test_data_dir_.AppendASCII("api_test")
                        .AppendASCII("browser_action")
                        .AppendASCII("none"));
  ASSERT_TRUE(extension.get());

  content::BrowserContext* browser_context = browser()->profile();
  {
    // Popup hosts are created with the correct type and profile.
    std::unique_ptr<ExtensionViewHost> host =
        ExtensionViewHostFactory::CreatePopupHost(extension->url(), browser());
    EXPECT_EQ(extension.get(), host->extension());
    EXPECT_EQ(browser_context, host->browser_context());
    EXPECT_EQ(mojom::ViewType::kExtensionPopup, host->extension_host_type());
  }

  {
    // Dialog hosts are created with the correct type and profile.
    std::unique_ptr<ExtensionViewHost> host =
        ExtensionViewHostFactory::CreateDialogHost(extension->url(),
                                                   browser()->profile());
    EXPECT_EQ(extension.get(), host->extension());
    EXPECT_EQ(browser_context, host->browser_context());
    EXPECT_EQ(mojom::ViewType::kExtensionDialog, host->extension_host_type());
  }
}

}  // namespace extensions
