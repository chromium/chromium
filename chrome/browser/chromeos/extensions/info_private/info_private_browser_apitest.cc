// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/test/browser_test.h"

class ChromeOSInfoPrivateBrowserTest : public extensions::ExtensionApiTest {
 public:
  ChromeOSInfoPrivateBrowserTest() = default;

  ChromeOSInfoPrivateBrowserTest(const ChromeOSInfoPrivateBrowserTest&) =
      delete;
  ChromeOSInfoPrivateBrowserTest& operator=(
      const ChromeOSInfoPrivateBrowserTest&) = delete;
  ~ChromeOSInfoPrivateBrowserTest() override = default;
};

IN_PROC_BROWSER_TEST_F(ChromeOSInfoPrivateBrowserTest, TestIsRunningOnLacros) {
  const char* custom_arg =
#if BUILDFLAG(IS_CHROMEOS_ASH)
      "isRunningOnLacros - False";
#else
      "isRunningOnLacros - True";
#endif

  ASSERT_TRUE(RunExtensionTest("chromeos_info_private/basic",
                               {.custom_arg = custom_arg},
                               {.load_as_component = true}))
      << message_;
}
