// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/new_tab_page_util.h"

#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/scoped_browser_locale.h"
#include "components/search/ntp_features.h"
#include "components/variations/service/variations_service.h"
#include "content/public/test/browser_test.h"

class NewTabPageUtilBrowserTest : public InProcessBrowserTest {
 protected:
  base::test::ScopedFeatureList features_;
};

class NewTabPageUtilEnableFlagBrowserTest : public NewTabPageUtilBrowserTest {
 public:
  NewTabPageUtilEnableFlagBrowserTest() {
    features_.InitWithFeatures(
        {ntp_features::kNtpRecipeTasksModule,
         ntp_features::kNtpChromeCartModule, ntp_features::kNtpDriveModule},
        {});
  }
};

class NewTabPageUtilDisableFlagBrowserTest : public NewTabPageUtilBrowserTest {
 public:
  NewTabPageUtilDisableFlagBrowserTest() {
    features_.InitWithFeatures({}, {ntp_features::kNtpRecipeTasksModule,
                                    ntp_features::kNtpChromeCartModule,
                                    ntp_features::kNtpDriveModule});
  }
};

IN_PROC_BROWSER_TEST_F(NewTabPageUtilBrowserTest, EnableRecipesByToT) {
  auto locale = std::make_unique<ScopedBrowserLocale>("en-US");
  g_browser_process->variations_service()->OverrideStoredPermanentCountry("us");
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  EXPECT_TRUE(IsRecipeTasksModuleEnabled());
#else
  EXPECT_FALSE(IsRecipeTasksModuleEnabled());
#endif
}

IN_PROC_BROWSER_TEST_F(NewTabPageUtilBrowserTest, DisableRecipesByToT) {
  auto locale = std::make_unique<ScopedBrowserLocale>("en-US");
  g_browser_process->variations_service()->OverrideStoredPermanentCountry("ca");
  EXPECT_FALSE(IsRecipeTasksModuleEnabled());
}

IN_PROC_BROWSER_TEST_F(NewTabPageUtilEnableFlagBrowserTest,
                       EnableRecipesByFlag) {
  EXPECT_TRUE(IsRecipeTasksModuleEnabled());
}

IN_PROC_BROWSER_TEST_F(NewTabPageUtilDisableFlagBrowserTest,
                       DisableRecipesByFlag) {
  auto locale = std::make_unique<ScopedBrowserLocale>("en-US");
  g_browser_process->variations_service()->OverrideStoredPermanentCountry("us");
  EXPECT_FALSE(IsRecipeTasksModuleEnabled());
}

IN_PROC_BROWSER_TEST_F(NewTabPageUtilBrowserTest, EnableCartByToT) {
  auto locale = std::make_unique<ScopedBrowserLocale>("en-US");
  g_browser_process->variations_service()->OverrideStoredPermanentCountry("us");
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  EXPECT_TRUE(IsCartModuleEnabled());
#else
  EXPECT_FALSE(IsCartModuleEnabled());
#endif
}

IN_PROC_BROWSER_TEST_F(NewTabPageUtilBrowserTest, DisableCartByToT) {
  auto locale = std::make_unique<ScopedBrowserLocale>("en-US");
  g_browser_process->variations_service()->OverrideStoredPermanentCountry("ca");
  EXPECT_FALSE(IsCartModuleEnabled());
}

IN_PROC_BROWSER_TEST_F(NewTabPageUtilEnableFlagBrowserTest, EnableCartByFlag) {
  EXPECT_TRUE(IsCartModuleEnabled());
}

IN_PROC_BROWSER_TEST_F(NewTabPageUtilDisableFlagBrowserTest,
                       DisableCartByFlag) {
  auto locale = std::make_unique<ScopedBrowserLocale>("en-US");
  g_browser_process->variations_service()->OverrideStoredPermanentCountry("us");
  EXPECT_FALSE(IsCartModuleEnabled());
}

IN_PROC_BROWSER_TEST_F(NewTabPageUtilBrowserTest, EnableDriveByToT) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  EXPECT_TRUE(IsDriveModuleEnabled());
#else
  EXPECT_FALSE(IsDriveModuleEnabled());
#endif
}

IN_PROC_BROWSER_TEST_F(NewTabPageUtilEnableFlagBrowserTest, EnableDriveByFlag) {
  EXPECT_TRUE(IsDriveModuleEnabled());
}

IN_PROC_BROWSER_TEST_F(NewTabPageUtilDisableFlagBrowserTest,
                       DisableDriveByFlag) {
  EXPECT_FALSE(IsDriveModuleEnabled());
}
