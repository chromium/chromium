// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/core/counters/bookmark_counter.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_future.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using browsing_data::BrowsingDataCounter;
typedef base::test::TestFuture<std::unique_ptr<BrowsingDataCounter::Result>>
    CounterFuture;
class BookmarkCounterTest : public testing::Test {
 public:
  BookmarkCounterTest() {
    TestingProfile::Builder profile_builder;
    profile_builder.AddTestingFactory(
        BookmarkModelFactory::GetInstance(),
        BookmarkModelFactory::GetDefaultFactory());
    profile_ = profile_builder.Build();

    model_ =
        BookmarkModelFactory::GetInstance()->GetForBrowserContext(profile());
  }

  Profile* profile() { return profile_.get(); }

  bookmarks::BookmarkModel* model() { return model_; }

  void AddNodes(const std::string& model_string) {
    bookmarks::test::AddNodesFromModelString(
        model(), model()->bookmark_bar_node(), model_string);
  }

  BrowsingDataCounter::ResultInt GetResultValue() {
    std::unique_ptr<BrowsingDataCounter::Result> result = future.Take();

    while (!result->Finished()) {
      future.Clear();
      result = future.Take();
    }

    BrowsingDataCounter::FinishedResult* finished_result =
        static_cast<BrowsingDataCounter::FinishedResult*>(result.get());
    return finished_result->Value();
  }

 protected:
  CounterFuture future;

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<bookmarks::BookmarkModel> model_;
};

TEST_F(BookmarkCounterTest, CountUnloaded) {
  ASSERT_FALSE(model()->loaded());
  browsing_data::BookmarkCounter counter(model());
  counter.InitWithoutPref(base::Time(), future.GetRepeatingCallback());
  counter.Restart();

  EXPECT_EQ(0, GetResultValue());
}

TEST_F(BookmarkCounterTest, Count) {
  bookmarks::test::WaitForBookmarkModelToLoad(model());
  ASSERT_TRUE(model()->loaded());
  AddNodes("1 2 3 ");
  browsing_data::BookmarkCounter counter(model());
  counter.InitWithoutPref(base::Time(), future.GetRepeatingCallback());
  counter.Restart();

  EXPECT_EQ(3, GetResultValue());

  AddNodes("5 6 ");
  counter.Restart();

  EXPECT_EQ(5, GetResultValue());
}

TEST_F(BookmarkCounterTest, CountWithPeriod) {
  bookmarks::test::WaitForBookmarkModelToLoad(model());
  base::Time now = base::Time::Now();
  AddNodes("1 2 3 ");
  GURL url("https://google.com");
  const bookmarks::BookmarkNode* node1 =
      model()->AddURL(model()->bookmark_bar_node(), 0, u"4", url);
  model()->SetDateAdded(node1, now - base::Minutes(30));
  const bookmarks::BookmarkNode* node2 =
      model()->AddURL(model()->bookmark_bar_node(), 0, u"5", url);
  model()->SetDateAdded(node2, now - base::Minutes(90));

  browsing_data::BookmarkCounter counter(model());
  counter.InitWithoutPref(now - base::Minutes(60),
                          future.GetRepeatingCallback());
  counter.Restart();

  // 1,2,3 and 4 should be counted. 5 is too old, so it will be skipped.
  EXPECT_EQ(4, GetResultValue());
}

TEST_F(BookmarkCounterTest, CountWithFolders) {
  bookmarks::test::WaitForBookmarkModelToLoad(model());
  AddNodes("1 2 3 f1:[ 4 5 f2:[ 6 ] ] ");
  browsing_data::BookmarkCounter counter(model());
  counter.InitWithoutPref(base::Time(), future.GetRepeatingCallback());
  counter.Restart();

  EXPECT_EQ(6, GetResultValue());
}

}  // namespace
