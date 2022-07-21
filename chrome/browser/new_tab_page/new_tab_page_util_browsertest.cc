// Copyright 2022 The Chromium Authors. All rights reserved.
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
    features_.InitWithFeatures({ntp_features::kNtpRecipeTasksModule}, {});
  }
};

class NewTabPageUtilDisableFlagBrowserTest : public NewTabPageUtilBrowserTest {
 public:
  NewTabPageUtilDisableFlagBrowserTest() {
    features_.InitWithFeatures({}, {ntp_features::kNtpRecipeTasksModule});
  }
};

IN_PROC_BROWSER_TEST_F(NewTabPageUtilBrowserTest, EnableByToT) {
  auto locale = std::make_unique<ScopedBrowserLocale>("en-US");
  g_browser_process->variations_service()->OverrideStoredPermanentCountry("us");
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  EXPECT_TRUE(IsRecipeTasksModuleEnabled());
#else
  EXPECT_FALSE(IsRecipeTasksModuleEnabled());
#endif
}

IN_PROC_BROWSER_TEST_F(NewTabPageUtilBrowserTest, DisableByToT) {
  auto locale = std::make_unique<ScopedBrowserLocale>("en-US");
  g_browser_process->variations_service()->OverrideStoredPermanentCountry("ca");
  EXPECT_FALSE(IsRecipeTasksModuleEnabled());
}

IN_PROC_BROWSER_TEST_F(NewTabPageUtilEnableFlagBrowserTest, EnableByFlag) {
  EXPECT_TRUE(IsRecipeTasksModuleEnabled());
}

IN_PROC_BROWSER_TEST_F(NewTabPageUtilDisableFlagBrowserTest, DisableByFlag) {
  auto locale = std::make_unique<ScopedBrowserLocale>("en-US");
  g_browser_process->variations_service()->OverrideStoredPermanentCountry("us");
  EXPECT_FALSE(IsRecipeTasksModuleEnabled());
}
