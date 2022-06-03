// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "content/public/test/browser_test.h"
#include "extensions/test/extension_test_message_listener.h"

namespace extensions {

// Tests that chrome://theme/ URLs are only accessible to component extensions.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest,
                       OnlyComponentExtensionsCanAccessChromeThemeUrls) {
  const base::FilePath extension_path(
      test_data_dir_.AppendASCII("browsertest")
                    .AppendASCII("chrome_theme_url"));
  ExtensionTestMessageListener listener(false);

  // First try loading the extension as a non-component extension.  The
  // chrome://theme/ image referenced in the extension should fail to load.
  const Extension* extension = LoadExtension(extension_path);
  ASSERT_TRUE(extension);
  EXPECT_TRUE(listener.WaitUntilSatisfied());
  EXPECT_EQ("not loaded", listener.message());

  // Unload the extension so we can reload it below with no chance of side
  // effects.
  extension_service()->UnloadExtension(extension->id(),
                                       UnloadedExtensionReason::UNINSTALL);
  listener.Reset();

  // Now try loading the extension as a component extension.  This time the
  // referenced image should load successfully.
  ASSERT_TRUE(LoadExtensionAsComponent(extension_path));
  EXPECT_TRUE(listener.WaitUntilSatisfied());
  EXPECT_EQ("loaded", listener.message());
}

}  // namespace extensions
