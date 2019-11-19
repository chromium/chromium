// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/history_tab_helper.h"

#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history/core/browser/history_constants.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/url_row.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class HistoryTabHelperTest : public ChromeRenderViewHostTestHarness {
 protected:
  HistoryTabHelperTest() {}

  // ChromeRenderViewHostTestHarness:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    ASSERT_TRUE(profile()->CreateHistoryService(/*delete_file=*/false,
                                                /*no_db=*/false));
    history_service_ = HistoryServiceFactory::GetForProfile(
        profile(), ServiceAccessType::IMPLICIT_ACCESS);
    ASSERT_TRUE(history_service_);
    history_service_->AddPage(
        page_url_, base::Time::Now(), /*context_id=*/nullptr,
        /*nav_entry_id=*/0,
        /*referrer=*/GURL(), history::RedirectList(), ui::PAGE_TRANSITION_TYPED,
        history::SOURCE_BROWSED, /*did_replace_entry=*/false);
    HistoryTabHelper::CreateForWebContents(web_contents());
  }

  HistoryTabHelper* history_tab_helper() {
    return HistoryTabHelper::FromWebContents(web_contents());
  }

  content::WebContentsTester* web_contents_tester() {
    return content::WebContentsTester::For(web_contents());
  }

  std::string QueryPageTitleFromHistory(const GURL& url) {
    std::string title;
    base::RunLoop loop;
    history_service_->QueryURL(
        url, /*want_visits=*/false,
        base::BindLambdaForTesting([&](history::QueryURLResult result) {
          EXPECT_TRUE(result.success);
          title = base::UTF16ToUTF8(result.row.title());
          loop.Quit();
        }),
        &tracker_);
    loop.Run();
    return title;
  }

  const GURL page_url_ = GURL("http://foo.com");

 private:
  base::CancelableTaskTracker tracker_;
  history::HistoryService* history_service_;

  DISALLOW_COPY_AND_ASSIGN(HistoryTabHelperTest);
};

TEST_F(HistoryTabHelperTest, ShouldUpdateTitleInHistory) {
  web_contents_tester()->NavigateAndCommit(page_url_);

  content::NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();
  ASSERT_NE(nullptr, entry);

  web_contents()->UpdateTitleForEntry(entry, base::UTF8ToUTF16("title1"));
  EXPECT_EQ("title1", QueryPageTitleFromHistory(page_url_));
}

TEST_F(HistoryTabHelperTest, ShouldLimitTitleUpdatesPerPage) {
  web_contents_tester()->NavigateAndCommit(page_url_);

  content::NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();
  ASSERT_NE(nullptr, entry);

  // The first 10 title updates are accepted and update history, as per
  // history::kMaxTitleChanges.
  for (int i = 1; i <= history::kMaxTitleChanges; ++i) {
    const std::string title = base::StringPrintf("title%d", i);
    web_contents()->UpdateTitleForEntry(entry, base::UTF8ToUTF16(title));
  }

  ASSERT_EQ("title10", QueryPageTitleFromHistory(page_url_));

  // Furhter updates should be ignored.
  web_contents()->UpdateTitleForEntry(entry, base::UTF8ToUTF16("title11"));
  EXPECT_EQ("title10", QueryPageTitleFromHistory(page_url_));
}

}  // namespace
