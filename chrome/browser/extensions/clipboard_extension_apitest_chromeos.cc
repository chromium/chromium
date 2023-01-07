// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/test/browser_test.h"
#include "extensions/test/extension_test_message_listener.h"

using ClipboardExtensionApiTest = extensions::ExtensionApiTest;

#if BUILDFLAG(IS_CHROMEOS)
// Disable due to flaky, https://crbug.com/1206809
IN_PROC_BROWSER_TEST_F(ClipboardExtensionApiTest,
                       DISABLED_ClipboardDataChanged) {
  ExtensionTestMessageListener result_listener("success 2");
  ASSERT_TRUE(RunExtensionTest("clipboard/clipboard_data_changed",
                               {.launch_as_platform_app = true}))
      << message_;
  ASSERT_TRUE(result_listener.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(ClipboardExtensionApiTest, SetImageData) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ExtensionTestMessageListener clipboard_change_listener(
      "clipboard data changed 2");
  ASSERT_TRUE(RunExtensionTest("clipboard/set_image_data",
                               {.launch_as_platform_app = true}))
      << message_;
  ASSERT_TRUE(clipboard_change_listener.WaitUntilSatisfied());
}

#endif  // BUILDFLAG(IS_CHROMEOS)
