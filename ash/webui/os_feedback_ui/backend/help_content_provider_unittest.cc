// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/os_feedback_ui/backend/help_content_provider.h"

#include <memory>
#include <string>

#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash {
namespace feedback {

using os_feedback_ui::mojom::HelpContent;
using os_feedback_ui::mojom::HelpContentType;
using os_feedback_ui::mojom::SearchRequest;
using os_feedback_ui::mojom::SearchResponse;
using os_feedback_ui::mojom::SearchResponsePtr;

class FakeHelpContentSearchService : public HelpContentSearchService {
 public:
  FakeHelpContentSearchService() = default;
  FakeHelpContentSearchService(const FakeHelpContentSearchService&) = delete;
  FakeHelpContentSearchService& operator=(const FakeHelpContentSearchService&) =
      delete;
  ~FakeHelpContentSearchService() override = default;

  void Search(const os_feedback_ui::mojom::SearchRequestPtr& request,
              os_feedback_ui::mojom::SearchResponsePtr& response) override {
    // Fake total results.
    response->total_results = 10;
    // Return 5 fake items.
    for (int i = 0; i < 5; i++) {
      // Fake title.
      const std::u16string title(
          base::StrCat({u"title", base::NumberToString16(i + 1)}));
      // Fake url.
      const GURL url(base::StrCat(
          {"https://help.com/?q=fakeurl", base::NumberToString(i + 1)}));
      response->results.emplace_back(HelpContent::New(
          title, url,
          i % 2 == 0 ? HelpContentType::kArticle : HelpContentType::kForum));
    }
  }
};

class HelpContentProviderTest : public testing::Test {
 public:
  HelpContentProviderTest()
      : provider_(std::make_unique<HelpContentProvider>(
            std::make_unique<FakeHelpContentSearchService>())) {}
  ~HelpContentProviderTest() override = default;

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<HelpContentProvider> provider_;
};

// Test that GetHelpContents returns a response with correct help contents.
TEST_F(HelpContentProviderTest, GetHelpContents) {
  base::RunLoop run_loop;

  auto request = SearchRequest::New(u"fake query", 5);
  provider_->GetHelpContents(
      std::move(request),
      base::BindLambdaForTesting([&](SearchResponsePtr response) {
        EXPECT_EQ(10u, response->total_results);
        EXPECT_EQ(5u, response->results.size());

        // It is sufficient by verifying the first and last item.
        EXPECT_EQ(u"title1", response->results[0]->title);
        EXPECT_EQ("https://help.com/?q=fakeurl1",
                  response->results[0]->url.spec());
        EXPECT_EQ(HelpContentType::kArticle,
                  response->results[0]->content_type);

        EXPECT_EQ(u"title5", response->results[4]->title);
        EXPECT_EQ("https://help.com/?q=fakeurl5",
                  response->results[4]->url.spec());
        EXPECT_EQ(HelpContentType::kArticle,
                  response->results[4]->content_type);

        run_loop.Quit();
      }));

  run_loop.Run();
}

}  // namespace feedback
}  // namespace ash
