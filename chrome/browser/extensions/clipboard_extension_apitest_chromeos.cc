// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/test/browser_test.h"
#include "extensions/test/extension_test_message_listener.h"

using ClipboardExtensionApiTest = extensions::ExtensionApiTest;

#if BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(ClipboardExtensionApiTest, ClipboardDataChanged) {
  ExtensionTestMessageListener result_listener("success 2", false);
  ASSERT_TRUE(RunExtensionTest({.name = "clipboard/clipboard_data_changed",
                                .launch_as_platform_app = true}))
      << message_;
  ASSERT_TRUE(result_listener.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(ClipboardExtensionApiTest, SetImageData) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ExtensionTestMessageListener clipboard_change_listener(
      "clipboard data changed 2", false);
  ASSERT_TRUE(RunExtensionTest(
      {.name = "clipboard/set_image_data", .launch_as_platform_app = true}))
      << message_;
  ASSERT_TRUE(clipboard_change_listener.WaitUntilSatisfied());
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
// Run the test for chrome.clipboard.setImageData, but do not wait for final
// clipboard change. This is because chrome.clipboard.onClipboardDataChanged is
// still unavailable.
// TODO(huangs): Unify this with the Ash version of the test once support for
// chrome.clipboard.onClipboardDataChanged has been added.
IN_PROC_BROWSER_TEST_F(ClipboardExtensionApiTest, SetImageData) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest(
      {.name = "clipboard/set_image_data", .launch_as_platform_app = true}))
      << message_;
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
