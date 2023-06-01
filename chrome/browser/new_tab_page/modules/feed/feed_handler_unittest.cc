// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/feed/feed_handler.h"

#include <memory>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/mock_callback.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/feed/core/common/pref_names.h"
#include "components/feed/core/proto/v2/wire/data_operation.pb.h"
#include "components/feed/core/proto/v2/wire/feed_response.pb.h"
#include "components/feed/core/proto/v2/wire/payload_metadata.pb.h"
#include "components/feed/core/proto/v2/wire/response.pb.h"
#include "components/feed/core/proto/v2/wire/stream_structure.pb.h"
#include "components/feed/core/v2/public/ntp_feed_content_fetcher.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "url/gurl.h"

namespace ntp::test {
namespace {

const char kExampleUrl[] = "http://example.com/";
const int kArticleCount = 3;

std::vector<::feed::NtpFeedContentFetcher::Article> MakeArticleList(int count) {
  std::vector<::feed::NtpFeedContentFetcher::Article> articles;
  for (int i = 0; i < count; ++i) {
    std::string number = base::NumberToString(i);
    auto& article = articles.emplace_back();
    article.title = base::StrCat({"Article ", number});
    article.publisher = base::StrCat({"Publisher ", number});
    article.url = GURL(base::StrCat({kExampleUrl, number}));
    article.thumbnail_url =
        GURL(base::StrCat({kExampleUrl, number, "/thumbnail.jpg"}));
    article.favicon_url =
        GURL(base::StrCat({kExampleUrl, number, "/favicon.ico"}));
  }
  return articles;
}

class TestNtpFeedContentFetcher : public ::feed::NtpFeedContentFetcher {
 public:
  TestNtpFeedContentFetcher()
      : ::feed::NtpFeedContentFetcher(
            /*identity_manager=*/nullptr,
            base::MakeRefCounted<network::TestSharedURLLoaderFactory>(),
            /*api_key=*/"api key",
            /*pref_service=*/nullptr) {}

  void FetchFollowingFeedArticles(
      base::OnceCallback<
          void(std::vector<::feed::NtpFeedContentFetcher::Article>)> callback)
      override {
    std::move(callback).Run(MakeArticleList(kArticleCount));
  }
};

}  // namespace

class FeedHandlerTest : public testing::Test {
 public:
  FeedHandlerTest()
      : testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    testing::Test::SetUp();
    ASSERT_TRUE(testing_profile_manager_.SetUp());
    profile_ = testing_profile_manager_.CreateTestingProfile("Test Profile");

    ::feed::RegisterProfilePrefs(profile_prefs_.registry());

    handler_ = std::make_unique<FeedHandler>(
        mojo::PendingReceiver<ntp::feed::mojom::FeedHandler>(),
        std::make_unique<TestNtpFeedContentFetcher>(), profile_);
  }

  void TearDown() override { handler_.reset(); }

 protected:
  std::unique_ptr<FeedHandler> handler_;
  raw_ptr<TestingProfile, DanglingUntriaged> profile_;
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  TestingProfileManager testing_profile_manager_;
  TestingPrefServiceSimple profile_prefs_;
  signin::IdentityTestEnvironment identity_test_env_;
};

TEST_F(FeedHandlerTest, GetFollowingFeedArticles) {
  std::vector<ntp::feed::mojom::ArticlePtr> actual_articles;
  base::MockCallback<
      ntp::feed::mojom::FeedHandler::GetFollowingFeedArticlesCallback>
      callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&](std::vector<ntp::feed::mojom::ArticlePtr> articles) {
            actual_articles = std::move(articles);
          }));

  // Run the above.
  handler_->GetFollowingFeedArticles(callback.Get());
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(3ul, actual_articles.size());
  auto& article = actual_articles.front();
  EXPECT_EQ("http://example.com/0", article->url);
  EXPECT_EQ("Article 0", article->title);
  EXPECT_EQ("Publisher 0", article->publisher);
  EXPECT_EQ("http://example.com/0/favicon.ico", article->favicon_url);
  EXPECT_EQ("http://example.com/0/thumbnail.jpg", article->thumbnail_url);
}

}  // namespace ntp::test
