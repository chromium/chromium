// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_browsertest.h"
#include "content/public/test/browser_test.h"
#include "extensions/test/result_catcher.h"

namespace extensions {

using ComponentExtensionBrowserTest = ExtensionBrowserTest;

// Tests that MojoJS is enabled for component extensions that need it.
// Note the test currently only runs for ChromeOS because the test extension
// uses `mojoPrivate` to test and `mojoPrivate` is ChromeOS only.
IN_PROC_BROWSER_TEST_F(ComponentExtensionBrowserTest, MojoJS) {
  ResultCatcher result_catcher;

  auto* extension =
      LoadExtension(test_data_dir_.AppendASCII("service_worker/mojo"),
                    {.load_as_component = true});
  ASSERT_TRUE(extension);

  ASSERT_TRUE(result_catcher.GetNextResult());
}

}  // namespace extensions
