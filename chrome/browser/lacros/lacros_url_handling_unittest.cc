// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/lacros_url_handling.h"

#include "base/test/task_environment.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/common/url_constants.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/startup/browser_init_params.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

TEST(LacrosUrlHandlingTest, IsURLAcceptedByAsh) {
  base::test::TaskEnvironment task_environment;

  auto params = crosapi::mojom::BrowserInitParams::New();
  params->accepted_internal_ash_urls = std::vector<GURL>{
      GURL(chrome::kChromeUIFlagsURL), GURL(chrome::kChromeUIOSSettingsURL),
      GURL("chrome://version"), GURL("chrome://settings/network")};
  chromeos::BrowserInitParams::SetInitParamsForTests(std::move(params));
  EXPECT_TRUE(lacros_url_handling::IsUrlAcceptedByAsh(
      GURL(chrome::kChromeUIOSSettingsURL)));
  EXPECT_TRUE(
      lacros_url_handling::IsUrlAcceptedByAsh(GURL(chrome::kChromeUIFlagsURL)));
  EXPECT_TRUE(lacros_url_handling::IsUrlAcceptedByAsh(
      GURL("chrome://settings/network")));
  EXPECT_TRUE(
      lacros_url_handling::IsUrlAcceptedByAsh(GURL("chrome://version")));
  EXPECT_FALSE(
      lacros_url_handling::IsUrlAcceptedByAsh(GURL("chrome://versions")));
  EXPECT_FALSE(lacros_url_handling::IsUrlAcceptedByAsh(GURL("http://version")));
  EXPECT_FALSE(lacros_url_handling::IsUrlAcceptedByAsh(GURL("")));
  EXPECT_FALSE(
      lacros_url_handling::IsUrlAcceptedByAsh(GURL("chrome://flags2")));
}

TEST(LacrosUrlHandlingTest, IsNavigationInterceptable) {
  // Here are the two originating URLs we are testing gainst.
  const GURL systemUrl = GURL("chrome://settings");
  const GURL normalUrl = GURL("https://www.google.com");
  Browser* browser = nullptr;
  // First check accpeptable cases:

  // User typed something.
  const NavigateParams typed = NavigateParams(
      browser, GURL(),
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                ui::PAGE_TRANSITION_FROM_ADDRESS_BAR));
  EXPECT_TRUE(lacros_url_handling::IsNavigationInterceptable(typed, normalUrl));
  // User used omnibox liink click.
  const NavigateParams omnibox = NavigateParams(
      browser, GURL(),
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_GENERATED |
                                ui::PAGE_TRANSITION_FROM_ADDRESS_BAR));
  EXPECT_TRUE(
      lacros_url_handling::IsNavigationInterceptable(omnibox, normalUrl));
  // User used a bookmark.
  const NavigateParams bookmark =
      NavigateParams(browser, GURL(), ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  EXPECT_TRUE(
      lacros_url_handling::IsNavigationInterceptable(bookmark, normalUrl));
  // User clicked on a link inside a system page.
  const NavigateParams syslink =
      NavigateParams(browser, GURL(), ui::PAGE_TRANSITION_LINK);
  EXPECT_TRUE(
      lacros_url_handling::IsNavigationInterceptable(syslink, systemUrl));

  // Check for unacceptable cases:

  // Want to follow link, but not coming from system page.
  EXPECT_FALSE(
      lacros_url_handling::IsNavigationInterceptable(syslink, normalUrl));

  // Any other combination.
  ui::PageTransition invalid_qualifier[] = {
      static_cast<ui::PageTransition>(0), ui::PAGE_TRANSITION_BLOCKED,
      ui::PAGE_TRANSITION_FORWARD_BACK,   ui::PAGE_TRANSITION_HOME_PAGE,
      ui::PAGE_TRANSITION_FROM_API,       ui::PAGE_TRANSITION_CHAIN_START,
      ui::PAGE_TRANSITION_CHAIN_END,      ui::PAGE_TRANSITION_CLIENT_REDIRECT};
  ui::PageTransition invalid_core[] = {ui::PAGE_TRANSITION_LINK,
                                       ui::PAGE_TRANSITION_AUTO_SUBFRAME,
                                       ui::PAGE_TRANSITION_MANUAL_SUBFRAME,
                                       ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                       ui::PAGE_TRANSITION_FORM_SUBMIT,
                                       ui::PAGE_TRANSITION_RELOAD,
                                       ui::PAGE_TRANSITION_KEYWORD,
                                       ui::PAGE_TRANSITION_KEYWORD_GENERATED};
  for (auto qualifier : invalid_qualifier) {
    const NavigateParams typed2 = NavigateParams(
        browser, GURL(),
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED | qualifier));
    EXPECT_FALSE(
        lacros_url_handling::IsNavigationInterceptable(typed2, systemUrl));
    const NavigateParams omnibox2 = NavigateParams(
        browser, GURL(),
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_GENERATED | qualifier));
    EXPECT_FALSE(
        lacros_url_handling::IsNavigationInterceptable(omnibox2, systemUrl));

    for (auto core : invalid_core) {
      // Check with normal source first.
      const NavigateParams other = NavigateParams(
          browser, GURL(), ui::PageTransitionFromInt(core | qualifier));
      EXPECT_FALSE(
          lacros_url_handling::IsNavigationInterceptable(other, normalUrl));
      // And then with a system source.
      if (PageTransitionCoreTypeIs(core, ui::PAGE_TRANSITION_LINK)) {
        EXPECT_FALSE(
            lacros_url_handling::IsNavigationInterceptable(other, normalUrl));
      } else {
        EXPECT_FALSE(
            lacros_url_handling::IsNavigationInterceptable(other, systemUrl));
      }
    }
  }
}
