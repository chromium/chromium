// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_ui_manager_delegate.h"

#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_host.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#endif

namespace safe_browsing {

class ChromeSafeBrowsingUIManagerDelegateTest
    : public ChromeRenderViewHostTestHarness {
 public:
  ChromeSafeBrowsingUIManagerDelegateTest() = default;
  ~ChromeSafeBrowsingUIManagerDelegateTest() override = default;
};

TEST_F(ChromeSafeBrowsingUIManagerDelegateTest, IsHostingExtension) {
  ChromeSafeBrowsingUIManagerDelegate delegate;

  // Sanity-check that vanilla WebContents instances are not marked as hosting
  // extensions.
  EXPECT_FALSE(delegate.IsHostingExtension(web_contents()));

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Create a WebContents instance that *is* hosting an extension.
  base::Value::Dict manifest;
  manifest.Set(extensions::manifest_keys::kName, "TestComponentApp");
  manifest.Set(extensions::manifest_keys::kVersion, "0.0.0.0");
  manifest.SetByDottedPath(
      extensions::manifest_keys::kPlatformAppBackgroundPage, std::string());
  std::string error;
  scoped_refptr<extensions::Extension> app;
  app = extensions::Extension::Create(
      base::FilePath(), extensions::mojom::ManifestLocation::kComponent,
      manifest, 0, &error);
  ASSERT_TRUE(app) << error;
  extensions::ProcessManager* extension_manager =
      extensions::ProcessManager::Get(web_contents()->GetBrowserContext());
  extension_manager->CreateBackgroundHost(app.get(), GURL("background.html"));
  extensions::ExtensionHost* host =
      extension_manager->GetBackgroundHostForExtension(app->id());
  auto* extension_web_contents = host->host_contents();

  // Check that the delegate flags this WebContents as hosting an extension.
  EXPECT_TRUE(delegate.IsHostingExtension(extension_web_contents));

  delete host;
#endif
}

}  // namespace safe_browsing
