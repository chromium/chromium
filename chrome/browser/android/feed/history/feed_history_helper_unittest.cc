// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/feed/history/feed_history_helper.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history/core/browser/history_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feed {

namespace {

const char kURL1[] = "http://foo.com";
const char kURL2[] = "http://bar.com";

}  // namespace

class FeedHistoryHelperTest : public testing::Test {
 protected:
  FeedHistoryHelperTest() {}

  void SetUp() override {
    ASSERT_TRUE(profile_.CreateHistoryService(/*delete_file=*/false,
                                              /*no_db=*/false));
    history_service_ = HistoryServiceFactory::GetForProfile(
        &profile_, ServiceAccessType::IMPLICIT_ACCESS);
    ASSERT_TRUE(history_service_);
    feed_history_helper_ =
        std::make_unique<FeedHistoryHelper>(history_service_);
    history_service_->AddPage(
        GURL(kURL1), base::Time(), /*context_id=*/nullptr,
        /*nav_entry_id=*/0,
        /*referrer=*/GURL(), history::RedirectList(), ui::PAGE_TRANSITION_TYPED,
        history::SOURCE_BROWSED, /*did_replace_entry=*/false);
  }

  void CheckURLExist(GURL url, bool expected) {
    base::RunLoop loop;
    feed_history_helper()->CheckURL(
        url, base::BindLambdaForTesting(([&](bool found) {
          EXPECT_EQ(expected, found);
          loop.Quit();
        })));
    loop.Run();
  }

  FeedHistoryHelper* feed_history_helper() {
    return feed_history_helper_.get();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  history::HistoryService* history_service_;
  TestingProfile profile_;
  std::unique_ptr<FeedHistoryHelper> feed_history_helper_;

  DISALLOW_COPY_AND_ASSIGN(FeedHistoryHelperTest);
};

TEST_F(FeedHistoryHelperTest, CheckURLSuccessTest) {
  CheckURLExist(GURL(kURL1), true);
}

TEST_F(FeedHistoryHelperTest, CheckURLFailureTest) {
  CheckURLExist(GURL(kURL2), false);
}

}  // namespace feed
