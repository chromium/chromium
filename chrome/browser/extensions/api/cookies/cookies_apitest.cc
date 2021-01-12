// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "net/cookies/cookie_util.h"

namespace extensions {

using ContextType = ExtensionApiTest::ContextType;

// This test cannot be run by a Service Worked-based extension
// because it uses the Document object.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, ReadFromDocument) {
  ASSERT_TRUE(RunExtensionTest("cookies/read_from_doc")) << message_;
}

class CookiesApiTest : public ExtensionApiTest,
                       public testing::WithParamInterface<ContextType> {
 protected:
  bool RunTest(const std::string& extension_name) {
    return RunTestWithFlags(extension_name, kFlagNone);
  }

  bool RunTestIncognito(const std::string& extension_name) {
    return RunTestWithFlags(extension_name, kFlagEnableIncognito);
  }

  bool RunTestWithArg(const std::string& extension_name,
                      const char* custom_arg) {
    int browser_test_flags = kFlagNone;
    if (GetParam() == ContextType::kServiceWorker)
      browser_test_flags |= kFlagRunAsServiceWorkerBasedExtension;

    return RunExtensionTestWithFlagsAndArg(extension_name, custom_arg,
                                           browser_test_flags, kFlagNone);
  }

  bool RunTestWithFlags(const std::string& extension_name,
                        int browser_test_flags) {
    if (GetParam() == ContextType::kServiceWorker)
      browser_test_flags |= kFlagRunAsServiceWorkerBasedExtension;

    return RunExtensionTestWithFlags(extension_name, browser_test_flags,
                                     kFlagNone);
  }
};

INSTANTIATE_TEST_SUITE_P(EventPage,
                         CookiesApiTest,
                         ::testing::Values(ContextType::kEventPage));
INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         CookiesApiTest,
                         ::testing::Values(ContextType::kServiceWorker));

IN_PROC_BROWSER_TEST_P(CookiesApiTest, Cookies) {
  ASSERT_TRUE(RunTestWithArg(
      "cookies/api",
      net::cookie_util::IsCookiesWithoutSameSiteMustBeSecureEnabled()
          ? "true"
          : "false"))
      << message_;
}

IN_PROC_BROWSER_TEST_P(CookiesApiTest, CookiesEvents) {
  ASSERT_TRUE(RunTest("cookies/events")) << message_;
}

IN_PROC_BROWSER_TEST_P(CookiesApiTest, CookiesEventsSpanning) {
  // We need to initialize an incognito mode window in order have an initialized
  // incognito cookie store. Otherwise, the chrome.cookies.set operation is just
  // ignored and we won't be notified about a newly set cookie for which we want
  // to test whether the storeId is set correctly.
  OpenURLOffTheRecord(browser()->profile(), GURL("chrome://newtab/"));
  ASSERT_TRUE(RunTestIncognito("cookies/events_spanning")) << message_;
}

IN_PROC_BROWSER_TEST_P(CookiesApiTest, CookiesNoPermission) {
  ASSERT_TRUE(RunTest("cookies/no_permission")) << message_;
}

}  // namespace extensions
