// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/tab_strip_context_decorator.h"

#include "base/test/test_future.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/contextual_task_context.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace contextual_tasks {

class TestTabStripContextDecorator : public TabStripContextDecorator {
 public:
  TestTabStripContextDecorator() : TabStripContextDecorator(nullptr) {}
  ~TestTabStripContextDecorator() override = default;

  std::vector<TabInfo> GetOpenTabUrls() override { return open_tabs_; }

  void SetOpenTabUrls(std::vector<TabInfo> open_tabs) {
    open_tabs_ = std::move(open_tabs);
  }

 private:
  std::vector<TabInfo> open_tabs_;
};

class TabStripContextDecoratorTest : public testing::Test {
 public:
  TabStripContextDecoratorTest() = default;
  ~TabStripContextDecoratorTest() override = default;

 protected:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(TabStripContextDecoratorTest, DecorateContextWithOpenTabs) {
  const GURL kOpenUrl("https://www.google.com");
  const std::u16string kOpenTitle = u"Google";
  const GURL kClosedUrl("https://www.youtube.com");
  const std::u16string kClosedTitle = u"YouTube";

  TestTabStripContextDecorator decorator;
  decorator.SetOpenTabUrls({{kOpenUrl, kOpenTitle}});

  ContextualTask task(base::Uuid::GenerateRandomV4());
  task.AddUrlResource(UrlResource(base::Uuid::GenerateRandomV4(), kOpenUrl));
  task.AddUrlResource(UrlResource(base::Uuid::GenerateRandomV4(), kClosedUrl));
  auto context = std::make_unique<ContextualTaskContext>(task);

  base::test::TestFuture<std::unique_ptr<ContextualTaskContext>> future;
  decorator.DecorateContext(std::move(context), nullptr, future.GetCallback());

  auto decorated_context = future.Take();
  ASSERT_TRUE(decorated_context);

  auto& attachments = decorated_context->GetMutableUrlAttachmentsForTesting();
  ASSERT_EQ(attachments.size(), 2u);

  auto& attachment1_data =
      attachments[0].GetMutableDecoratorDataForTesting().tab_strip_data;
  EXPECT_TRUE(attachment1_data.is_open_in_tab_strip);
  EXPECT_EQ(attachment1_data.title, kOpenTitle);

  auto& attachment2_data =
      attachments[1].GetMutableDecoratorDataForTesting().tab_strip_data;
  EXPECT_FALSE(attachment2_data.is_open_in_tab_strip);
  EXPECT_TRUE(attachment2_data.title.empty());
}

TEST_F(TabStripContextDecoratorTest, DecorateContextWithDeduplicatedUrls) {
  const GURL kOpenUrl("https://www.google.com");
  const std::u16string kOpenTitle = u"Google";
  const GURL kSemanticallySimilarUrl("https://google.com");
  const GURL kClosedUrl("https://www.youtube.com");
  const std::u16string kClosedTitle = u"YouTube";

  TestTabStripContextDecorator decorator;
  decorator.SetOpenTabUrls({{kOpenUrl, kOpenTitle}});

  ContextualTask task(base::Uuid::GenerateRandomV4());
  task.AddUrlResource(
      UrlResource(base::Uuid::GenerateRandomV4(), kSemanticallySimilarUrl));
  task.AddUrlResource(UrlResource(base::Uuid::GenerateRandomV4(), kClosedUrl));
  auto context = std::make_unique<ContextualTaskContext>(task);

  base::test::TestFuture<std::unique_ptr<ContextualTaskContext>> future;
  decorator.DecorateContext(std::move(context), nullptr, future.GetCallback());

  auto decorated_context = future.Take();
  ASSERT_TRUE(decorated_context);

  auto& attachments = decorated_context->GetMutableUrlAttachmentsForTesting();
  ASSERT_EQ(attachments.size(), 2u);

  auto& attachment1_data =
      attachments[0].GetMutableDecoratorDataForTesting().tab_strip_data;
  EXPECT_TRUE(attachment1_data.is_open_in_tab_strip);
  EXPECT_EQ(attachment1_data.title, kOpenTitle);

  auto& attachment2_data =
      attachments[1].GetMutableDecoratorDataForTesting().tab_strip_data;
  EXPECT_FALSE(attachment2_data.is_open_in_tab_strip);
  EXPECT_TRUE(attachment2_data.title.empty());
}

}  // namespace contextual_tasks
