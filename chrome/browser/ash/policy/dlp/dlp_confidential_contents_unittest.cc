// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/dlp_confidential_contents.h"

#include <string>

#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace policy {

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

 private:
  content::BrowserTaskEnvironment task_environment_;
  const std::unique_ptr<TestingProfile> profile_;
};

TEST_F(DlpConfidentialContentsTest, EmptyContents) {
  DlpConfidentialContents contents;
  EXPECT_TRUE(contents.IsEmpty());
}

TEST_F(DlpConfidentialContentsTest, DuplicateConfidentialDataAdded) {
  DlpConfidentialContents contents;
  contents.Add(CreateWebContents(title1, url1).get());
  contents.Add(CreateWebContents(title1, url1).get());
  EXPECT_EQ(contents.GetContents().size(), 1);
  EXPECT_EQ(contents.GetContents().begin()->title, title1);
  EXPECT_EQ(contents.GetContents().begin()->url, url1);
}

TEST_F(DlpConfidentialContentsTest, ClearAndAdd) {
  DlpConfidentialContents contents;

  contents.Add(CreateWebContents(title1, url1).get());
  contents.Add(CreateWebContents(title2, url2).get());
  EXPECT_EQ(contents.GetContents().size(), 2);

  contents.ClearAndAdd(CreateWebContents(title3, url3).get());
  EXPECT_EQ(contents.GetContents().size(), 1);
  EXPECT_EQ(contents.GetContents().begin()->title, title3);
  EXPECT_EQ(contents.GetContents().begin()->url, url3);
}

TEST_F(DlpConfidentialContentsTest, UnionShouldAddUniqueItems) {
  DlpConfidentialContents contents1;
  DlpConfidentialContents contents2;

  contents1.Add(CreateWebContents(title1, url1).get());
  contents1.Add(CreateWebContents(title2, url2).get());

  contents2.Add(CreateWebContents(title1, url1).get());
  contents2.Add(CreateWebContents(title3, url3).get());

  EXPECT_EQ(contents1.GetContents().size(), 2);
  EXPECT_EQ(contents2.GetContents().size(), 2);

  contents1.UnionWith(contents2);

  EXPECT_EQ(contents1.GetContents().size(), 3);
  EXPECT_EQ(contents2.GetContents().size(), 2);
}

TEST_F(DlpConfidentialContentsTest, DifferenceShouldRemoveMatchedItems) {
  DlpConfidentialContents contents1;
  DlpConfidentialContents contents2;

  contents1.Add(CreateWebContents(title1, url1).get());
  contents1.Add(CreateWebContents(title2, url2).get());

  contents2.Add(CreateWebContents(title1, url1).get());
  contents2.Add(CreateWebContents(title3, url3).get());

  EXPECT_EQ(contents1.GetContents().size(), 2);
  EXPECT_EQ(contents2.GetContents().size(), 2);

  contents1.DifferenceWith(contents2);

  EXPECT_EQ(contents1.GetContents().size(), 1);
  EXPECT_EQ(contents1.GetContents().begin()->title, title2);
  EXPECT_EQ(contents1.GetContents().begin()->url, url2);
  EXPECT_EQ(contents2.GetContents().size(), 2);
}

}  // namespace policy
