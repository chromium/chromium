// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_service_util.h"

#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/scoped_browser_locale.h"
#include "components/sync/base/features.h"
#include "components/variations/service/variations_service.h"
#include "content/public/test/browser_test.h"

class SyncSeviceUtilBrowserTest : public InProcessBrowserTest {
 public:
  SyncSeviceUtilBrowserTest() = default;
};

IN_PROC_BROWSER_TEST_F(SyncSeviceUtilBrowserTest,
                       SyncPollEnabledBasedOnPlatformAndCountryLocale) {
  auto locale = std::make_unique<ScopedBrowserLocale>("en-US");
  g_browser_process->variations_service()->OverrideStoredPermanentCountry("us");
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_WIN)
  EXPECT_TRUE(IsDesktopEnUSLocaleOnlySyncPollFeatureEnabled());
#else
  EXPECT_FALSE(IsDesktopEnUSLocaleOnlySyncPollFeatureEnabled());
#endif
}

IN_PROC_BROWSER_TEST_F(SyncSeviceUtilBrowserTest,
                       SyncPollDisabledBasedOnPlatformAndCountryLocale) {
  auto locale = std::make_unique<ScopedBrowserLocale>("en-US");
  g_browser_process->variations_service()->OverrideStoredPermanentCountry("ca");
  EXPECT_FALSE(IsDesktopEnUSLocaleOnlySyncPollFeatureEnabled());
}
