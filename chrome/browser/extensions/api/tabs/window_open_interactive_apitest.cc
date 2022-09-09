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

// The test uses the chrome.browserAction.openPopup API, which requires that the
// window can automatically be activated.
// Fails flakily on Linux and Lacros. https://crbug.com/477691.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
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
