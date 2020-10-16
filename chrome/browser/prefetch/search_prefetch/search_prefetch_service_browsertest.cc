// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/prefetch/search_prefetch/field_trial_settings.h"
#include "chrome/browser/prefetch/search_prefetch/search_prefetch_service.h"
#include "chrome/browser/prefetch/search_prefetch/search_prefetch_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

class SearchPrefetchServiceDisabledBrowserTest : public InProcessBrowserTest {
 public:
  SearchPrefetchServiceDisabledBrowserTest() {
    feature_list_.InitAndDisableFeature(kSearchPrefetchService);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SearchPrefetchServiceDisabledBrowserTest,
                       ServiceNotCreatedWhenDisabled) {
  EXPECT_EQ(nullptr,
            SearchPrefetchServiceFactory::GetForProfile(browser()->profile()));
}

class SearchPrefetchServiceEnabledBrowserTest : public InProcessBrowserTest {
 public:
  SearchPrefetchServiceEnabledBrowserTest() {
    feature_list_.InitAndEnableFeature(kSearchPrefetchService);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SearchPrefetchServiceEnabledBrowserTest,
                       ServiceNotCreatedWhenIncognito) {
  EXPECT_EQ(nullptr, SearchPrefetchServiceFactory::GetForProfile(
                         browser()->profile()->GetPrimaryOTRProfile()));
}

IN_PROC_BROWSER_TEST_F(SearchPrefetchServiceEnabledBrowserTest,
                       ServiceCreatedWhenFeatureEnabled) {
  EXPECT_NE(nullptr,
            SearchPrefetchServiceFactory::GetForProfile(browser()->profile()));
}
