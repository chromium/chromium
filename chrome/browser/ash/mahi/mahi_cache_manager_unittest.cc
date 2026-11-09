// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/mahi/mahi_cache_manager.h"

#include <map>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
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
    return std::map<GURL, MahiCacheManager::MahiData>(
        mahi_cache_manager_->page_cache_.begin(),
        mahi_cache_manager_->page_cache_.end());
  }

  const MahiCacheManager::MahiData& GetCacheEntry(const GURL& url) {
    auto it = mahi_cache_manager_->page_cache_.Peek(url);
    CHECK(it != mahi_cache_manager_->page_cache_.end());
    return it->second;
  }

  // testing::Test:
  void SetUp() override {
    mahi_cache_manager_ = std::make_unique<MahiCacheManager>();
    mahi_cache_manager_->AddCacheForUrl(
        "http://url1.com",
        MahiCacheManager::MahiData(
            "http://url1.com", u"title 1", u"page content 1",
            /* favicon_image = */ std::nullopt, u"summary 1",
            {{u"Question 1", u"Answer 1"}, {u"Question 2", u"Answer 2"}}));

    // Next item in the cache is logged 10 minutes later.
    task_environment_.FastForwardBy(base::Minutes(10));

    mahi_cache_manager_->AddCacheForUrl(
        "http://url2.com", MahiCacheManager::MahiData(
                               "http://url2.com", u"title 2", u"page content 2",
                               /* favicon_image = */ std::nullopt, u"summary 2",
                               {{u"question 1", u"answer 1"}}));
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
  EXPECT_EQ(GetPageCache().at(GURL("http://url1.com")).GetPageContent(),
            new_data.GetPageContent());
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

  // Fast forward 19 minutes (29 minutes after the first entry was added). At
  // this point no cache is expired yet.
  task_environment_.FastForwardBy(base::Minutes(19));
  EXPECT_EQ(GetMahiCacheManager()->size(), 2);

  // Two minutes later, the first cache has expired. Expired entries are removed
  // on access.
  task_environment_.FastForwardBy(base::Minutes(2));
  EXPECT_FALSE(GetMahiCacheManager()->GetSummaryForUrl("http://url1.com"));
  EXPECT_EQ(GetPageCache().size(), 1u);
  auto result = GetMahiCacheManager()->GetSummaryForUrl("http://url2.com");
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), u"summary 2");

  // Ten minutes later, the second cache has expired too.
  task_environment_.FastForwardBy(base::Minutes(10));
  EXPECT_EQ(GetMahiCacheManager()->GetPageContentForUrl("http://url2.com"),
            u"");
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

TEST_F(MahiCacheManagerTest, StoresPageContentCompressed) {
  // Repetitive text with non-ASCII characters and an unpaired surrogate, which
  // must round-trip losslessly.
  std::u16string content;
  for (int i = 0; i < 1000; ++i) {
    content += u"Mahi page content \u65e5\u672c\u8a9e \xD800. ";
  }
  GetMahiCacheManager()->AddCacheForUrl(
      "https://example.com", {"https://example.com", u"title", content,
                              /*favicon_image=*/std::nullopt, u"summary",
                              /*previous_qa=*/{}});

  const auto& entry = GetCacheEntry(GURL("https://example.com/"));
  EXPECT_LT(entry.compressed_page_content.size(),
            content.size() * sizeof(char16_t) / 10);
  EXPECT_EQ(GetMahiCacheManager()->GetPageContentForUrl("https://example.com"),
            content);
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

TEST_F(MahiCacheManagerTest, EvictsLeastRecentlyUsedEntriesBeyondMaxCapacity) {
  GetMahiCacheManager()->ClearCache();

  auto make_url = [](size_t index) {
    return base::StrCat({"https://example.com/", base::NumberToString(index)});
  };
  auto add_entry = [&](size_t index) {
    const std::string url = make_url(index);
    const std::u16string index_u16 = base::NumberToString16(index);
    GetMahiCacheManager()->AddCacheForUrl(
        url,
        {url, base::StrCat({u"title ", index_u16}),
         base::StrCat({u"content ", index_u16}),
         /*favicon_image=*/std::nullopt, base::StrCat({u"summary ", index_u16}),
         /*previous_qa=*/{}});
  };

  for (size_t i = 0; i < MahiCacheManager::kMaxCacheEntries; ++i) {
    add_entry(i);
  }
  EXPECT_EQ(GetPageCache().size(), MahiCacheManager::kMaxCacheEntries);

  // Accessing entries 0 and 1 promotes them to most-recently-used.
  EXPECT_EQ(GetMahiCacheManager()->GetSummaryForUrl(make_url(0)), u"summary 0");
  EXPECT_EQ(GetMahiCacheManager()->GetPageContentForUrl(make_url(1)),
            u"content 1");

  // Adding two new entries beyond capacity should evict entries 2 and 3.
  add_entry(MahiCacheManager::kMaxCacheEntries);
  add_entry(MahiCacheManager::kMaxCacheEntries + 1);

  EXPECT_EQ(GetPageCache().size(), MahiCacheManager::kMaxCacheEntries);
  EXPECT_FALSE(
      GetMahiCacheManager()->GetSummaryForUrl(make_url(2)).has_value());
  EXPECT_FALSE(
      GetMahiCacheManager()->GetSummaryForUrl(make_url(3)).has_value());
  EXPECT_EQ(GetMahiCacheManager()->GetSummaryForUrl(make_url(0)), u"summary 0");
  EXPECT_EQ(GetMahiCacheManager()->GetSummaryForUrl(make_url(1)), u"summary 1");
  EXPECT_EQ(
      GetMahiCacheManager()->GetSummaryForUrl(
          make_url(MahiCacheManager::kMaxCacheEntries)),
      base::StrCat({u"summary ", base::NumberToString16(
                                     MahiCacheManager::kMaxCacheEntries)}));
  EXPECT_EQ(
      GetMahiCacheManager()->GetSummaryForUrl(
          make_url(MahiCacheManager::kMaxCacheEntries + 1)),
      base::StrCat({u"summary ", base::NumberToString16(
                                     MahiCacheManager::kMaxCacheEntries + 1)}));
}

}  // namespace ash
