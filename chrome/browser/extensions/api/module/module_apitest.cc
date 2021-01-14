// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/test/browser_test.h"

using ExtensionModuleApiTest = extensions::ExtensionApiTest;

IN_PROC_BROWSER_TEST_F(ExtensionModuleApiTest, CognitoFile) {
  ASSERT_TRUE(RunExtensionTest("extension_module/cognito_file")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionModuleApiTest, IncognitoFile) {
  ASSERT_TRUE(RunExtensionTestIncognito(
      "extension_module/incognito_file")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionModuleApiTest, CognitoNoFile) {
  ASSERT_TRUE(RunExtensionTestNoFileAccess(
      "extension_module/cognito_nofile")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionModuleApiTest, IncognitoNoFile) {
  ASSERT_TRUE(RunExtensionTestIncognitoNoFileAccess(
      "extension_module/incognito_nofile")) << message_;
}
