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
  bool RunTest(const char* extension_name,
               bool allow_in_incognito = false,
               const char* custom_arg = nullptr) {
    return RunExtensionTest(
        {.name = extension_name, .custom_arg = custom_arg},
        {.allow_in_incognito = allow_in_incognito,
         .load_as_service_worker = GetParam() == ContextType::kServiceWorker});
  }
};

INSTANTIATE_TEST_SUITE_P(EventPage,
                         CookiesApiTest,
                         ::testing::Values(ContextType::kEventPage));
INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         CookiesApiTest,
                         ::testing::Values(ContextType::kServiceWorker));

IN_PROC_BROWSER_TEST_P(CookiesApiTest, Cookies) {
  ASSERT_TRUE(
      RunTest("cookies/api", /*allow_in_incognito=*/false,
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
  ASSERT_TRUE(RunTest("cookies/events_spanning",
                      /*allow_in_incognito=*/true))
      << message_;
}

IN_PROC_BROWSER_TEST_P(CookiesApiTest, CookiesNoPermission) {
  ASSERT_TRUE(RunTest("cookies/no_permission")) << message_;
}

}  // namespace extensions
