// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/test/browser_test.h"
#include "extensions/buildflags/buildflags.h"

// TODO(crbug.com/371432155): Remove this file when chrome.tabs API is
// available on desktop Android. These tests only exist to help maintain the
// stub tabs API.
static_assert(BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS));

namespace extensions {

using TabsApiTest = ExtensionApiTest;

// Verifies basics like opening a tab and receiving an update event message.
// This ensure most of the plumbing is hooked up in the stub.
IN_PROC_BROWSER_TEST_F(TabsApiTest, SmokeTest) {
  ASSERT_TRUE(RunExtensionTest("tabs/smoke")) << message_;
}

// Regression test for crbug.com/459932363. We should keep this test when the
// rest of the file is deleted.
IN_PROC_BROWSER_TEST_F(TabsApiTest, ZoomTest) {
  ASSERT_TRUE(RunExtensionTest("tabs/zoom")) << message_;
}

}  // namespace extensions
