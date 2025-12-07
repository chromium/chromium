// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/test/browser_test.h"
#include "extensions/test/result_catcher.h"

namespace extensions {

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, WindowOpenFocus) {
  ASSERT_TRUE(RunExtensionTest("window_open/focus")) << message_;
}

// TODO(crbug.com/414913454): Disabled on LSAN builds due to consistent failure.
#if defined(ADDRESS_SANITIZER) || defined(LEAK_SANITIZER)
#define MAYBE_WindowOpen DISABLED_WindowOpen
#else
#define MAYBE_WindowOpen WindowOpen
#endif
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, MAYBE_WindowOpen) {
  extensions::ResultCatcher catcher;
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("window_open").AppendASCII("spanning"),
      {.allow_in_incognito = true}));
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

}  // namespace extensions
