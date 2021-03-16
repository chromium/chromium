// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/test/browser_test.h"
#include "extensions/test/result_catcher.h"

namespace extensions {

namespace {

using SearchApiTest = ExtensionApiTest;

// Test various scenarios, such as the use of input different parameters.
IN_PROC_BROWSER_TEST_F(SearchApiTest, Normal) {
  ASSERT_TRUE(RunExtensionTest("search/query/normal")) << message_;
}

// Test incognito browser in extension default spanning mode.
// Disabled due to flakes on Ozone testers; see https://crbug.com/1188651.
#if defined(USE_OZONE)
#define MAYBE_Incognito DISABLED_Incognito
#else
#define MAYBE_Incognito Incognito
#endif
IN_PROC_BROWSER_TEST_F(SearchApiTest, MAYBE_Incognito) {
  ResultCatcher catcher;
  CreateIncognitoBrowser(browser()->profile());
  ASSERT_TRUE(RunExtensionTest({.name = "search/query/incognito"},
                               {.allow_in_incognito = true}))
      << message_;
}

// Test incognito browser in extension split mode.
IN_PROC_BROWSER_TEST_F(SearchApiTest, IncognitoSplit) {
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(
      browser()->profile()->GetPrimaryOTRProfile());
  CreateIncognitoBrowser(browser()->profile());
  ASSERT_TRUE(RunExtensionTest({.name = "search/query/incognito_split"},
                               {.allow_in_incognito = true}))
      << message_;
}

}  // namespace
}  // namespace extensions
