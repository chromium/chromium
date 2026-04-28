// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/test/browser_test.h"
#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS));

namespace extensions {

using TabsApiTest = ExtensionApiTest;

// Regression test for crbug.com/459932363, specific to android.
IN_PROC_BROWSER_TEST_F(TabsApiTest, ZoomTest) {
  ASSERT_TRUE(RunExtensionTest("tabs/zoom")) << message_;
}

}  // namespace extensions
