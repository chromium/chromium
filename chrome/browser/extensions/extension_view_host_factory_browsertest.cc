// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_view_host_factory.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_view_host.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "content/public/test/browser_test.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension.h"
#include "extensions/common/mojom/view_type.mojom.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

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

  content::BrowserContext* browser_context = profile();

  // Popup hosts are created with the correct type and profile.
  std::unique_ptr<ExtensionViewHost> host =
      ExtensionViewHostFactory::CreatePopupHost(*extension, extension->url(),
                                                GetBrowserWindowInterface());
  EXPECT_EQ(extension.get(), host->extension());
  EXPECT_EQ(browser_context, host->browser_context());
  EXPECT_EQ(mojom::ViewType::kExtensionPopup, host->extension_host_type());
}

// TODO(crbug.com/458688998): Desktop Android doesn't support side panel yet.
#if BUILDFLAG(ENABLE_EXTENSIONS)

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

  content::BrowserContext* browser_context = profile();

  {
    // Create a side panel host with a browser passed in.
    std::unique_ptr<ExtensionViewHost> host =
        ExtensionViewHostFactory::CreateSidePanelHost(
            *extension, extension->url(), browser(),
            /*tab_interface=*/nullptr);
    EXPECT_EQ(extension.get(), host->extension());
    EXPECT_EQ(browser_context, host->browser_context());
    EXPECT_EQ(mojom::ViewType::kExtensionSidePanel,
              host->extension_host_type());
  }

  {
    // Create a side panel host with a tab based WebContents passed in.
    auto* browser = GetBrowserWindowInterface();
    auto* tab_list = TabListInterface::From(browser);
    auto* active_tab = tab_list->GetActiveTab();
    std::unique_ptr<ExtensionViewHost> host =
        ExtensionViewHostFactory::CreateSidePanelHost(
            *extension, extension->url(), /*browser=*/nullptr, active_tab);
    EXPECT_EQ(extension.get(), host->extension());
    EXPECT_EQ(browser_context, host->browser_context());
    EXPECT_EQ(mojom::ViewType::kExtensionSidePanel,
              host->extension_host_type());
  }
}

#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

}  // namespace extensions
