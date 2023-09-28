// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/ntp_tiles/chrome_most_visited_sites_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/history/core/browser/top_sites.h"
#include "components/ntp_tiles/most_visited_sites.h"
#include "components/ntp_tiles/ntp_tile.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ntp_tiles {

namespace {

using testing::Contains;
using testing::Not;

std::string PrintTile(const std::string& title,
                      const std::string& url,
                      TileSource source) {
  return std::string("has title \"") + title + std::string("\" and url \"") +
         url + std::string("\" and source ") +
         testing::PrintToString(static_cast<int>(source));
}

MATCHER_P3(MatchesTile, title, url, source, PrintTile(title, url, source)) {
  return arg.title == base::ASCIIToUTF16(title) && arg.url == GURL(url) &&
         arg.source == source;
}

// Waits for most visited URLs to be made available.
class MostVisitedSitesWaiter : public MostVisitedSites::Observer {
 public:
  MostVisitedSitesWaiter() : tiles_(NTPTilesVector()) {}

  // Waits until most visited URLs are available, and then returns all the
  // tiles.
  NTPTilesVector WaitForTiles() {
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
    return tiles_;
  }

  void OnURLsAvailable(
      const std::map<SectionType, NTPTilesVector>& sections) override {
    tiles_ = sections.at(SectionType::PERSONALIZED);
    if (quit_closure_) {
      std::move(quit_closure_).Run();
    }
  }

  void OnIconMadeAvailable(const GURL& site_url) override {}

 private:
  base::OnceClosure quit_closure_;
  NTPTilesVector tiles_;
};

}  // namespace

class NTPTilesTest : public InProcessBrowserTest {
 public:
  NTPTilesTest() {}

 protected:
  void SetUpOnMainThread() override {
    most_visited_sites_ =
        ChromeMostVisitedSitesFactory::NewForProfile(browser()->profile());
  }

  void TearDownOnMainThread() override {
    // Reset most_visited_sites_, otherwise there is a CHECK in callback_list.h
    // because callbacks_.size() is not 0.
    most_visited_sites_.reset();
  }

  std::unique_ptr<ntp_tiles::MostVisitedSites> most_visited_sites_;
};

// Tests that after navigating to a URL, ntp tiles will include the URL.
IN_PROC_BROWSER_TEST_F(NTPTilesTest, LoadURL) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL page_url = embedded_test_server()->GetURL("/simple.html");

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), page_url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  MostVisitedSitesWaiter waiter;

  // This call will call SyncWithHistory(), which means the new URL will be in
  // the next set of tiles that the waiter retrieves.
  most_visited_sites_->AddMostVisitedURLsObserver(&waiter, /*max_num_sites=*/8);

  NTPTilesVector tiles = waiter.WaitForTiles();
  EXPECT_THAT(tiles, Contains(MatchesTile("OK", page_url.spec().c_str(),
                                          TileSource::TOP_SITES)));
}

// Tests that after navigating to a URL with a server redirect, ntp tiles will
// include the correct URL.
IN_PROC_BROWSER_TEST_F(NTPTilesTest, ServerRedirect) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL final_url = embedded_test_server()->GetURL("/defaultresponse");
  GURL first_url =
      embedded_test_server()->GetURL("/server-redirect?" + final_url.spec());

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), first_url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  MostVisitedSitesWaiter waiter;
  most_visited_sites_->AddMostVisitedURLsObserver(&waiter, /*max_num_sites=*/8);

  NTPTilesVector tiles = waiter.WaitForTiles();

  // TopSites uses the start of the redirect chain, so verify the first URL
  // is listed, but not the final URL.
  EXPECT_THAT(tiles, Contains(MatchesTile("", first_url.spec().c_str(),
                                          TileSource::TOP_SITES)));
  EXPECT_THAT(tiles, Not(Contains(MatchesTile("", final_url.spec().c_str(),
                                              TileSource::TOP_SITES))));
}

// Tests usage of MostVisitedSites mimicking Chrome Home, where an observer is
// installed early and once and navigations follow afterwards.
// Flaky on several platforms: https://crbug.com/1487047.
IN_PROC_BROWSER_TEST_F(NTPTilesTest, DISABLED_NavigateAfterSettingObserver) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL page_url = embedded_test_server()->GetURL("/simple.html");

  // Register the observer before doing the navigation.
  MostVisitedSitesWaiter waiter;
  most_visited_sites_->AddMostVisitedURLsObserver(&waiter, /*max_num_sites=*/8);

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), page_url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  most_visited_sites_->Refresh();
  NTPTilesVector tiles = waiter.WaitForTiles();
  EXPECT_THAT(tiles, Contains(MatchesTile("OK", page_url.spec().c_str(),
                                          TileSource::TOP_SITES)));
}

}  // namespace ntp_tiles
