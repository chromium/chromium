// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_browsertest.h"
#include "content/public/test/browser_test.h"
#include "extensions/test/result_catcher.h"

namespace extensions {

using AppWindowRestrictedApisBrowserTest = ExtensionBrowserTest;

// Test that the window events like onbeforeunload event are correctly
// clobbered.
IN_PROC_BROWSER_TEST_F(AppWindowRestrictedApisBrowserTest, UnloadEvents) {
  ResultCatcher catcher;
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("app_forbidden_apis/onbeforeunload")));
  ASSERT_TRUE(catcher.GetNextResult());
}

// Test that Document apis like document.write are correctly clobbered.
IN_PROC_BROWSER_TEST_F(AppWindowRestrictedApisBrowserTest, DocumentApis) {
  ResultCatcher catcher;
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("app_forbidden_apis/document_apis")));
  ASSERT_TRUE(catcher.GetNextResult());
}

}  // namespace extensions
