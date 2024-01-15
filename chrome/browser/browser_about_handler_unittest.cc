// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_about_handler.h"

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "chrome/common/url_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/common/referrer.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using content::BrowserThread;
using content::NavigationController;
using content::NavigationEntry;
using content::Referrer;

namespace {
struct AboutURLTestCase {
  GURL test_url;
  GURL expected_url;
};
}

class BrowserAboutHandlerTest : public testing::Test {
 protected:
  void TestHandleChromeAboutAndChromeSyncRewrite(
      const std::vector<AboutURLTestCase>& test_cases) {
    TestingProfile profile;

    for (const auto& test_case : test_cases) {
      GURL url(test_case.test_url);
      HandleChromeAboutAndChromeSyncRewrite(&url, &profile);
      EXPECT_EQ(test_case.expected_url, url);
    }
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(BrowserAboutHandlerTest, HandleChromeAboutAndChromeSyncRewrite) {
  std::string chrome_prefix(content::kChromeUIScheme);
  chrome_prefix.append(url::kStandardSchemeSeparator);
  std::vector<AboutURLTestCase> test_cases(
      {{GURL("http://google.com"), GURL("http://google.com")},
       {GURL(url::kAboutBlankURL), GURL(url::kAboutBlankURL)},
       {GURL(chrome_prefix + chrome::kChromeUIDefaultHost),
        GURL(chrome_prefix + chrome::kChromeUIVersionHost)},
       {GURL(chrome_prefix + chrome::kChromeUIAboutHost),
        GURL(chrome_prefix + chrome::kChromeUIChromeURLsHost)},
       {GURL(chrome_prefix + chrome::kChromeUISignInInternalsHost),
        GURL(chrome_prefix + chrome::kChromeUISignInInternalsHost)},
       {GURL(chrome_prefix + chrome::kChromeUISyncHost),
        GURL(chrome_prefix + chrome::kChromeUISyncInternalsHost)},
       {
           GURL(chrome_prefix + "host/path?query#ref"),
           GURL(chrome_prefix + "host/path?query#ref"),
       }});
  TestHandleChromeAboutAndChromeSyncRewrite(test_cases);
}

TEST_F(BrowserAboutHandlerTest,
       HandleChromeAboutAndChromeSyncRewriteForMDSettings) {
  std::string chrome_prefix(content::kChromeUIScheme);
  chrome_prefix.append(url::kStandardSchemeSeparator);
  std::vector<AboutURLTestCase> test_cases(
      {{GURL(chrome_prefix + chrome::kChromeUISettingsHost),
        GURL(chrome_prefix + chrome::kChromeUISettingsHost)}});
  TestHandleChromeAboutAndChromeSyncRewrite(test_cases);
}

TEST_F(BrowserAboutHandlerTest,
       HandleChromeAboutAndChromeSyncRewriteForHistory) {
  GURL::Replacements replace_foo_query;
  replace_foo_query.SetQueryStr("foo");
  GURL history_foo_url(
      GURL(chrome::kChromeUIHistoryURL).ReplaceComponents(replace_foo_query));
  TestHandleChromeAboutAndChromeSyncRewrite(std::vector<AboutURLTestCase>({
      {GURL("chrome:history"), GURL(chrome::kChromeUIHistoryURL)},
      {GURL(chrome::kChromeUIHistoryURL), GURL(chrome::kChromeUIHistoryURL)},
      {history_foo_url, history_foo_url},
  }));
}

// Ensure that minor BrowserAboutHandler fixup to a URL does not cause us to
// keep a separate virtual URL, which would not be updated on redirects.
// See https://crbug.com/449829.
TEST_F(BrowserAboutHandlerTest, NoVirtualURLForFixup) {
  GURL url("view-source:http://.foo");

  // No "fixing" of the URL is expected at the content::NavigationEntry layer.
  // We should only "fix" strings from the user (e.g. URLs from the Omnibox).
  //
  // Rewriters will remove the view-source prefix and expect it to stay in the
  // virtual URL.
  GURL expected_virtual_url = url;
  GURL expected_url("http://.foo/");

  TestingProfile profile;
  std::unique_ptr<NavigationEntry> entry(
      NavigationController::CreateNavigationEntry(
          url, Referrer(), /* initiator_origin= */ std::nullopt,
          /* initiator_base_url= */ std::nullopt, ui::PAGE_TRANSITION_RELOAD,
          false, std::string(), &profile,
          nullptr /* blob_url_loader_factory */));
  EXPECT_EQ(expected_virtual_url, entry->GetVirtualURL());
  EXPECT_EQ(expected_url, entry->GetURL());
}

TEST_F(BrowserAboutHandlerTest, HandleNonNavigationAboutURL_Invalid) {
  GURL invalid_url("https:");
  ASSERT_FALSE(invalid_url.is_valid());
  EXPECT_FALSE(HandleNonNavigationAboutURL(invalid_url));
}
