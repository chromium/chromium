// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/mahi/mahi_cache_manager.h"

#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class MahiCacheManagerTest : public testing::Test {
 public:
  MahiCacheManagerTest() = default;

  MahiCacheManagerTest(const MahiCacheManagerTest&) = delete;
  MahiCacheManagerTest& operator=(const MahiCacheManagerTest&) = delete;

  ~MahiCacheManagerTest() override = default;

  MahiCacheManager* GetMahiCacheManager() { return mahi_cache_manager_.get(); }

  std::map<GURL, MahiCacheManager::MahiData> GetPageCache() {
    return mahi_cache_manager_->page_cache_;
  }

  // testing::Test:
  void SetUp() override {
    mahi_cache_manager_ = std::make_unique<MahiCacheManager>();
    mahi_cache_manager_->page_cache_[GURL("http://url1.com/")] =
        MahiCacheManager::MahiData(
            "http://url1.com", u"title 1", u"page content 1",
            /* favicon_image = */ std::nullopt, u"summary 1",
            {{u"Question 1", u"Answer 1"}, {u"Question 2", u"Answer 2"}});

    // Next item in the cache is logged 5 hours later.
    task_environment_.FastForwardBy(base::Hours(5));

    mahi_cache_manager_->page_cache_[GURL("http://url2.com/")] =
        MahiCacheManager::MahiData(
            "http://url2.com", u"title 2", u"page content 2",
            /* favicon_image = */ std::nullopt, u"summary 2",
            {{u"question 1", u"answer 1"}});
  }

  void TearDown() override { mahi_cache_manager_.reset(); }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  std::unique_ptr<MahiCacheManager> mahi_cache_manager_;
};

TEST_F(MahiCacheManagerTest, AddNewURL) {
  EXPECT_EQ(GetPageCache().size(), 2u);
  GetMahiCacheManager()->AddCacheForUrl("http://url3.com",
                                        {"http://url3.com",
                                         u"title 3",
                                         u"page content 3",
                                         /* favicon_image = */ std::nullopt,
                                         u"summary 3",
                                         {{u"new question", u"new answer"}}});
  EXPECT_EQ(GetPageCache().size(), 3u);
}

TEST_F(MahiCacheManagerTest, ReplacingExistingURLWithNewContent) {
  MahiCacheManager::MahiData new_data(
      "http://url1.com", u"New title", u"New content",
      /* favicon_image = */ std::nullopt, u"New summary",
      {{u"new question", u"new answer"}});
  GetMahiCacheManager()->AddCacheForUrl("http://url1.com", new_data);
  EXPECT_EQ(GetPageCache().size(), 2u);
  EXPECT_EQ(GetPageCache().at(GURL("http://url1.com")).url, new_data.url);
  EXPECT_EQ(GetPageCache().at(GURL("http://url1.com")).title, new_data.title);
  EXPECT_EQ(GetPageCache().at(GURL("http://url1.com")).page_content,
            new_data.page_content);
  EXPECT_EQ(GetPageCache().at(GURL("http://url1.com")).summary,
            new_data.summary);
  EXPECT_EQ(GetPageCache().at(GURL("http://url1.com")).previous_qa.size(), 1u);
}

TEST_F(MahiCacheManagerTest, ReplaceSummaryWithExistingUrl) {
  EXPECT_EQ(GetPageCache().size(), 2u);
  GetMahiCacheManager()->TryToUpdateSummaryForUrl("http://url1.com",
                                                  u"new summary");
  auto result = GetMahiCacheManager()->GetSummaryForUrl("http://url1.com");
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), u"new summary");
  EXPECT_EQ(GetPageCache().size(), 2u);
}

TEST_F(MahiCacheManagerTest, UpdateSummaryDoesNothingIfURLIsntInCache) {
  EXPECT_EQ(GetPageCache().size(), 2u);
  GetMahiCacheManager()->TryToUpdateSummaryForUrl("http://not-there.com",
                                                  u"new summary");
  auto result = GetMahiCacheManager()->GetSummaryForUrl("http://not-there.com");
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(GetPageCache().size(), 2u);
}

TEST_F(MahiCacheManagerTest, GetSummaryFromTheCacheSameURL) {
  auto result = GetMahiCacheManager()->GetSummaryForUrl("http://url1.com");
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), u"summary 1");
}

TEST_F(MahiCacheManagerTest, GetSummaryFromTheCacheDifferentURL) {
  auto result =
      GetMahiCacheManager()->GetSummaryForUrl("http://url1.com/search");
  EXPECT_FALSE(result.has_value());
}

TEST_F(MahiCacheManagerTest, GetSummaryFromTheCacheWithRef) {
  auto result = GetMahiCacheManager()->GetSummaryForUrl("http://url1.com/#ref");
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), u"summary 1");
}

TEST_F(MahiCacheManagerTest, GetQAFromCacheSameURL) {
  auto result = GetMahiCacheManager()->GetQAForUrl("http://url1.com");
  EXPECT_EQ(result.size(), 2u);
  EXPECT_EQ(result[0].question, u"Question 1");
  EXPECT_EQ(result[0].answer, u"Answer 1");
  EXPECT_EQ(result[1].question, u"Question 2");
  EXPECT_EQ(result[1].answer, u"Answer 2");
}

TEST_F(MahiCacheManagerTest, GetQAFromCacheURLWithRef) {
  auto result = GetMahiCacheManager()->GetQAForUrl("http://url1.com/#ref");
  EXPECT_EQ(result.size(), 2u);
  EXPECT_EQ(result[0].question, u"Question 1");
  EXPECT_EQ(result[0].answer, u"Answer 1");
  EXPECT_EQ(result[1].question, u"Question 2");
  EXPECT_EQ(result[1].answer, u"Answer 2");
}

TEST_F(MahiCacheManagerTest, ClearCacheSuccessfully) {
  // Current cache size.
  EXPECT_EQ(GetPageCache().size(), 2u);

  // Clear the cache.
  GetMahiCacheManager()->ClearCache();
  EXPECT_EQ(GetPageCache().size(), 0u);
}

TEST_F(MahiCacheManagerTest, CorrectlyClearCacheWithRetention) {
  // Current cache size.
  EXPECT_EQ(GetPageCache().size(), 2u);

  // Fast forward 162h (24 * 7 - 6). At this point no cache is deleted yet.
  task_environment_.FastForwardBy(base::Hours(24 * 7 - 6));
  EXPECT_EQ(GetPageCache().size(), 2u);

  // Two hours later, the first cache should be deleted.
  task_environment_.FastForwardBy(base::Hours(2));
  EXPECT_EQ(GetPageCache().size(), 1u);
  auto result = GetMahiCacheManager()->GetSummaryForUrl("http://url2.com");
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), u"summary 2");

  // Four hours later, the second cache should be deleted.
  task_environment_.FastForwardBy(base::Hours(4));
  EXPECT_EQ(GetPageCache().size(), 0u);
}

TEST_F(MahiCacheManagerTest, GetCorrectPageContentFromURL) {
  // Get correct content if the url is found.
  auto result = GetMahiCacheManager()->GetPageContentForUrl("http://url1.com");
  EXPECT_EQ(result, u"page content 1");

  // No url is found
  result = GetMahiCacheManager()->GetPageContentForUrl("http://notfound.com");
  EXPECT_EQ(result, u"");
}

TEST_F(MahiCacheManagerTest, DeleteExistingURL) {
  // Current cache size.
  EXPECT_EQ(GetPageCache().size(), 2u);

  GetMahiCacheManager()->DeleteCacheForUrl("http://url1.com");
  EXPECT_EQ(GetPageCache().size(), 1u);
}

TEST_F(MahiCacheManagerTest, DeleteURLNotInCache) {
  // Current cache size.
  EXPECT_EQ(GetPageCache().size(), 2u);

  GetMahiCacheManager()->DeleteCacheForUrl("http://notthere.com");
  EXPECT_EQ(GetPageCache().size(), 2u);
}

TEST_F(MahiCacheManagerTest, OnlyStoreHTTPOrHTTPS) {
  EXPECT_EQ(GetPageCache().size(), 2u);

  GetMahiCacheManager()->AddCacheForUrl("file:///file/path",
                                        {"file:///file/path",
                                         u"local file",
                                         u"local content",
                                         /* favicon_image = */ std::nullopt,
                                         u"summary",
                                         {{u"new question", u"new answer"}}});

  EXPECT_EQ(GetPageCache().size(), 2u);
}

}  // namespace ash
