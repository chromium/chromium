// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_confidential_contents.h"

#include <sstream>
#include <string>

#include "base/ranges/algorithm.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_mock_time_task_runner.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "components/enterprise/data_controls/core/browser/dlp_histogram_helper.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace policy {

constexpr DlpRulesManager::Restriction kRestriction =
    DlpRulesManager::Restriction::kPrinting;

class DlpConfidentialContentsTest : public testing::Test {
 public:
  const std::u16string title1 = u"Example1";
  const std::u16string title2 = u"Example2";
  const std::u16string title3 = u"Example3";
  const GURL url1 = GURL("https://example1.com");
  const GURL url2 = GURL("https://example2.com");
  const GURL url3 = GURL("https://example3.com");

  DlpConfidentialContentsTest()
      : profile_(std::make_unique<TestingProfile>()) {}
  DlpConfidentialContentsTest(const DlpConfidentialContentsTest&) = delete;
  DlpConfidentialContentsTest& operator=(const DlpConfidentialContentsTest&) =
      delete;
  ~DlpConfidentialContentsTest() override = default;

  std::unique_ptr<content::WebContents> CreateWebContents(
      const std::u16string& title,
      const GURL& url) {
    std::unique_ptr<content::WebContents> web_contents =
        content::WebContentsTester::CreateTestWebContents(profile_.get(),
                                                          nullptr);
    content::WebContentsTester* tester =
        content::WebContentsTester::For(web_contents.get());
    tester->SetTitle(title);
    tester->SetLastCommittedURL(url);
    return web_contents;
  }

  DlpConfidentialContent CreateConfidentialContent(const std::u16string& title,
                                                   const GURL& url) {
    return DlpConfidentialContent(CreateWebContents(title, url).get());
  }

  // Helper to check whether |contents| has an element corresponding to
  // |web_contents|.
  bool Contains(const DlpConfidentialContents& contents,
                content::WebContents* web_contents) {
    return base::ranges::any_of(contents.GetContents(),
                                [&](const DlpConfidentialContent& content) {
                                  return content.url.EqualsIgnoringRef(
                                      web_contents->GetLastCommittedURL());
                                });
  }

 protected:
  base::HistogramTester histogram_tester_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler render_view_host_test_enabler_;
  const std::unique_ptr<TestingProfile> profile_;
};

TEST_F(DlpConfidentialContentsTest, ComparisonWithDifferentUrls) {
  DlpConfidentialContent content1 = CreateConfidentialContent(title1, url1);
  DlpConfidentialContent content2 = CreateConfidentialContent(title2, url2);

  EXPECT_TRUE(content1 != content2);
  EXPECT_TRUE(content1 < content2);
  EXPECT_TRUE(content1 <= content2);
  EXPECT_TRUE(content2 > content1);
  EXPECT_TRUE(content2 >= content1);
}

TEST_F(DlpConfidentialContentsTest, ComparisonWithSameUrls) {
  DlpConfidentialContent content1 = CreateConfidentialContent(title1, url1);
  DlpConfidentialContent content2 = CreateConfidentialContent(title2, url1);

  EXPECT_TRUE(content1 == content2);
  EXPECT_FALSE(content1 != content2);
  EXPECT_FALSE(content1 < content2);
  EXPECT_TRUE(content1 <= content2);
  EXPECT_FALSE(content2 > content1);
  EXPECT_TRUE(content2 >= content1);
}

TEST_F(DlpConfidentialContentsTest, ComparisonAfterAssignement) {
  DlpConfidentialContent content1 = CreateConfidentialContent(title1, url1);
  DlpConfidentialContent content2 = CreateConfidentialContent(title2, url2);

  EXPECT_FALSE(content1 == content2);

  content2 = content1;
  EXPECT_TRUE(content1 == content2);
}

TEST_F(DlpConfidentialContentsTest, EqualityIgnoresTheRef) {
  const GURL url1 = GURL("https://example.com#first_ref");
  const GURL url2 = GURL("https://example.com#second_ref");
  EXPECT_NE(url1, url2);
  EXPECT_TRUE(url1.EqualsIgnoringRef(url2));

  DlpConfidentialContent content1 = CreateConfidentialContent(title1, url1);
  DlpConfidentialContent content2 = CreateConfidentialContent(title2, url2);
  EXPECT_EQ(content1, content2);
}

TEST_F(DlpConfidentialContentsTest, EmptyContents) {
  DlpConfidentialContents contents;
  EXPECT_TRUE(contents.IsEmpty());
}

TEST_F(DlpConfidentialContentsTest, DuplicateConfidentialDataAdded) {
  DlpConfidentialContents contents;
  auto web_contents = CreateWebContents(title1, url1);
  contents.Add(web_contents.get());
  contents.Add(web_contents.get());
  EXPECT_EQ(contents.GetContents().size(), 1u);
  EXPECT_TRUE(Contains(contents, web_contents.get()));
}

TEST_F(DlpConfidentialContentsTest, ClearAndAdd) {
  DlpConfidentialContents contents;

  auto web_contents1 = CreateWebContents(title1, url1);
  auto web_contents2 = CreateWebContents(title2, url2);
  auto web_contents3 = CreateWebContents(title3, url3);

  contents.Add(web_contents1.get());
  contents.Add(web_contents2.get());
  EXPECT_EQ(contents.GetContents().size(), 2u);

  contents.ClearAndAdd(CreateWebContents(title3, url3).get());
  EXPECT_EQ(contents.GetContents().size(), 1u);
  EXPECT_FALSE(Contains(contents, web_contents1.get()));
  EXPECT_FALSE(Contains(contents, web_contents2.get()));
  EXPECT_TRUE(Contains(contents, web_contents3.get()));
}

TEST_F(DlpConfidentialContentsTest, InsertOrUpdateDropsDuplicates) {
  DlpConfidentialContents contents1;
  DlpConfidentialContents contents2;

  auto web_contents1 = CreateWebContents(title1, url1);
  auto web_contents2 = CreateWebContents(title2, url2);
  auto web_contents3 = CreateWebContents(title3, url3);

  contents1.Add(web_contents1.get());
  contents1.Add(web_contents2.get());

  contents2.Add(web_contents1.get());
  contents2.Add(web_contents3.get());

  EXPECT_EQ(contents1.GetContents().size(), 2u);
  EXPECT_EQ(contents2.GetContents().size(), 2u);
  EXPECT_FALSE(Contains(contents1, web_contents3.get()));

  contents1.InsertOrUpdate(contents2);

  EXPECT_EQ(contents1.GetContents().size(), 3u);
  EXPECT_EQ(contents2.GetContents().size(), 2u);
  EXPECT_TRUE(Contains(contents1, web_contents3.get()));
}

TEST_F(DlpConfidentialContentsTest, InsertOrUpdateUpdatesTitles) {
  const GURL url1 = GURL("https://example.com#first_ref");
  const GURL url2 = GURL("https://example.com#second_ref");
  EXPECT_NE(url1, url2);
  EXPECT_TRUE(url1.EqualsIgnoringRef(url2));

  DlpConfidentialContents contents1;
  DlpConfidentialContents contents2;

  auto web_contents1 = CreateWebContents(title1, url1);
  auto web_contents2 = CreateWebContents(title2, url2);
  auto web_contents3 = CreateWebContents(title3, url3);

  contents1.Add(web_contents1.get());
  contents2.Add(web_contents2.get());
  contents2.Add(web_contents3.get());

  EXPECT_EQ(contents1.GetContents().begin()->title, title1);

  contents1.InsertOrUpdate(contents2);

  EXPECT_EQ(contents2.GetContents().size(), 2u);
  EXPECT_EQ(contents1.GetContents().begin()->title, title2);
}

TEST_F(DlpConfidentialContentsTest, EqualityDoesNotDependOnOrder) {
  DlpConfidentialContents contents1;
  DlpConfidentialContents contents2;

  auto web_contents1 = CreateWebContents(title1, url1);
  auto web_contents2 = CreateWebContents(title2, url2);
  auto web_contents3 = CreateWebContents(title3, url3);

  contents1.Add(web_contents1.get());
  contents1.Add(web_contents2.get());
  contents2.Add(web_contents2.get());
  contents2.Add(web_contents1.get());
  EXPECT_TRUE(contents1 == contents2);
  EXPECT_TRUE(EqualWithTitles(contents1, contents2));

  contents1.Add(web_contents3.get());
  EXPECT_TRUE(contents1 != contents2);
  EXPECT_FALSE(EqualWithTitles(contents1, contents2));
}

TEST_F(DlpConfidentialContentsTest, EqualityDoesNotDependOnTitle) {
  DlpConfidentialContents contents1;
  DlpConfidentialContents contents2;
  DlpConfidentialContents contents3;

  auto web_contents1 = CreateWebContents(title1, url1);
  auto web_contents2 = CreateWebContents(title2, url1);

  contents1.Add(web_contents1.get());
  contents2.Add(web_contents1.get());
  contents3.Add(web_contents2.get());
  EXPECT_TRUE(contents1 == contents2);
  EXPECT_TRUE(EqualWithTitles(contents1, contents2));

  EXPECT_TRUE(contents1 == contents3);
  EXPECT_FALSE(EqualWithTitles(contents1, contents3));
}

TEST_F(DlpConfidentialContentsTest, CacheEvictsAfterTimeout) {
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner =
      base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  DlpConfidentialContentsCache cache;
  cache.SetTaskRunnerForTesting(task_runner);

  DlpConfidentialContent content = CreateConfidentialContent(title1, url1);

  cache.Cache(content, kRestriction);
  EXPECT_TRUE(cache.Contains(content, kRestriction));
  histogram_tester_.ExpectBucketCount(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kConfidentialContentsCount,
      1, 1);
  task_runner->FastForwardBy(DlpConfidentialContentsCache::GetCacheTimeout());
  EXPECT_FALSE(cache.Contains(content, kRestriction));
}

TEST_F(DlpConfidentialContentsTest, CacheEvictsWhenFull) {
  DlpConfidentialContentsCache cache;
  DlpConfidentialContent content1 = CreateConfidentialContent(title1, url1);
  cache.Cache(content1, kRestriction);
  histogram_tester_.ExpectBucketCount(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kConfidentialContentsCount,
      1, 1);
  for (int i = 2; i <= 100; i++) {
    std::stringstream url;
    url << "https://example";
    url << i;
    url << ".com";
    cache.Cache(CreateConfidentialContent(u"title", GURL(url.str())),
                kRestriction);
  }
  EXPECT_EQ(cache.GetSizeForTesting(), 100u);
  histogram_tester_.ExpectBucketCount(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kConfidentialContentsCount,
      100, 1);
  EXPECT_EQ(histogram_tester_
                .GetHistogramSamplesSinceCreation(
                    data_controls::GetDlpHistogramPrefix() +
                    data_controls::dlp::kConfidentialContentsCount)
                ->TotalCount(),
            100);

  // Add an additional item which should lead to the first one being evicted.
  DlpConfidentialContent content101 =
      CreateConfidentialContent(u"Example101", GURL("https://example101.com"));
  cache.Cache(content101, kRestriction);
  EXPECT_EQ(cache.GetSizeForTesting(), 100u);
  EXPECT_FALSE(cache.Contains(content1, kRestriction));
  EXPECT_TRUE(cache.Contains(content101, kRestriction));
  EXPECT_EQ(histogram_tester_
                .GetHistogramSamplesSinceCreation(
                    data_controls::GetDlpHistogramPrefix() +
                    data_controls::dlp::kConfidentialContentsCount)
                ->TotalCount(),
            101);
  histogram_tester_.ExpectBucketCount(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kConfidentialContentsCount,
      100, 2);
}

TEST_F(DlpConfidentialContentsTest, CacheRemovesDuplicates) {
  DlpConfidentialContentsCache cache;

  DlpConfidentialContent content = CreateConfidentialContent(title1, url1);

  cache.Cache(content, kRestriction);
  cache.Cache(content, kRestriction);
  EXPECT_EQ(cache.GetSizeForTesting(), 1u);
  histogram_tester_.ExpectBucketCount(
      data_controls::GetDlpHistogramPrefix() +
          data_controls::dlp::kConfidentialContentsCount,
      1, 1);
}

}  // namespace policy
