// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/test/result_catcher.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

namespace {

using SearchApiTest = ExtensionApiTest;

// TODO(crbug.com/434262354):Enable this test once chrome.tabs(create, query,
// onUpdated, etc.) is supported on desktop Android.
#if BUILDFLAG(ENABLE_EXTENSIONS)
// Test various scenarios, such as the use of input different parameters.
// Disabled due to flakes on Mac and Win testers; see
// https://crbug.com/394345948.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
#define MAYBE_Normal DISABLED_Normal
#else
#define MAYBE_Normal Normal
#endif
IN_PROC_BROWSER_TEST_F(SearchApiTest, MAYBE_Normal) {
  ASSERT_TRUE(RunExtensionTest("search/query/normal")) << message_;
}

// Test incognito browser in extension default spanning mode.
IN_PROC_BROWSER_TEST_F(SearchApiTest, Incognito) {
  ResultCatcher catcher;
  auto* incognito_web_contents =
      PlatformOpenURLOffTheRecord(profile(), GURL("about:blank"));
  auto* incognito_context = incognito_web_contents->GetBrowserContext();
  ASSERT_TRUE(incognito_context->IsOffTheRecord());
  ASSERT_TRUE(RunExtensionTest("search/query/incognito", {},
                               {.allow_in_incognito = true}))
      << message_;
}

// Test incognito browser in extension split mode.
IN_PROC_BROWSER_TEST_F(SearchApiTest, IncognitoSplit) {
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true));
  auto* incognito_web_contents =
      PlatformOpenURLOffTheRecord(profile(), GURL("about:blank"));
  auto* incognito_context = incognito_web_contents->GetBrowserContext();
  ASSERT_TRUE(incognito_context->IsOffTheRecord());
  ASSERT_TRUE(RunExtensionTest("search/query/incognito_split", {},
                               {.allow_in_incognito = true}))
      << message_;
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

}  // namespace
}  // namespace extensions
