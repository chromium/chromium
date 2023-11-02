// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/test/browser_test.h"

namespace extensions {

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, FileAPI) {
  ASSERT_TRUE(RunExtensionTest("fileapi")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, XHROnPersistentFileSystem) {
  ASSERT_TRUE(
      RunExtensionTest("xhr_persistent_fs", {.launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, RequestQuotaInBackgroundPage) {
  ASSERT_TRUE(RunExtensionTest("request_quota_background")) << message_;
}

}  // namespace extensions
