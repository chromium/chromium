// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/os_feedback_ui/backend/help_content_provider.h"

#include <memory>
#include <string>

#include "ash/webui/os_feedback_ui/mojom/os_feedback_ui.mojom.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/google_api_keys.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash {
namespace feedback {

using data_decoder::test::InProcessDataDecoder;
using os_feedback_ui::mojom::HelpContent;
using os_feedback_ui::mojom::HelpContentPtr;
using os_feedback_ui::mojom::HelpContentType;
using os_feedback_ui::mojom::SearchRequest;
using os_feedback_ui::mojom::SearchRequestPtr;
using os_feedback_ui::mojom::SearchResponse;
using os_feedback_ui::mojom::SearchResponsePtr;

constexpr char kFakeResponse[] = R"({"resource": [
    {
      "language": "en-US",
      "url": "/chromebook/fake1?hl=en-gb",
      "title": "fake-title-1",
      "resultType": "CT_ANSWER"
    },
    {
      "language": "zh-Hans",
      "resultType": "CT_ANSWER",
      "title": "将Chromecast 与Chromebook 搭配使用",
      "url": "/chromebook/answer/3289520?hl=zh-Hans"
    },
    {
      "language": "en-gb",
      "url": "https://support.google.com/chromebook/fake2?hl=en-gb",
      "title": "fake-title-2",
      "resultType": "CT_SUPPORT_FORUM_THREAD"
    }
  ],
  "totalResults": "2000000"})";

class HelpContentProviderTest : public testing::Test {
 public:
  HelpContentProviderTest() {
    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
  }
  ~HelpContentProviderTest() override = default;

  void SetUp() override {
    in_process_data_decoder_ = std::make_unique<InProcessDataDecoder>();
  }

  const std::string GetApiUrl() const {
    return base::StrCat(
        {"https://scone-pa.clients6.google.com/v1/search/list?key=",
         google_apis::GetAPIKey()});
  }

  // Call the GetHelpContents of the remote provider async and return the
  // response.
  SearchResponsePtr GetHelpContentsAndWait(SearchRequestPtr request) {
    base::test::TestFuture<SearchResponsePtr> response;
    provider_remote_->GetHelpContents(std::move(request),
                                      response.GetCallback());
    return response.Take();
  }

  // Initialize provider.
  void InitializeProvider(const bool is_child_account) {
    provider_ = std::make_unique<HelpContentProvider>(
        "en", is_child_account, test_shared_loader_factory_);
    provider_->BindInterface(provider_remote_.BindNewPipeAndPassReceiver());
  }

  // Parse the json and call PopulateSearchResponse if successful.
  void PopulateSearchResponseHelper(const std::string& json,
                                    SearchResponsePtr& search_response) {
    std::optional<base::Value> search_result = base::JSONReader::Read(json);
    if (search_result) {
      PopulateSearchResponse("en-gb", /*is_child_account=*/false, 5u,
                             search_result.value(), search_response);
    }
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<data_decoder::test::InProcessDataDecoder>
      in_process_data_decoder_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;

  std::unique_ptr<HelpContentProvider> provider_;
  mojo::Remote<os_feedback_ui::mojom::HelpContentProvider> provider_remote_;
};

// Test the ToHelpContentType utility function.
TEST_F(HelpContentProviderTest, ConvertToHelpContentType) {
  EXPECT_EQ(HelpContentType::kArticle, ToHelpContentType("CT_ANSWER"));

  EXPECT_EQ(HelpContentType::kForum, ToHelpContentType("CT_FORUM_CONTENT"));
  EXPECT_EQ(HelpContentType::kForum,
            ToHelpContentType(" CT_SUPPORT_FORUM_NEW_THREAD"));
  EXPECT_EQ(HelpContentType::kForum,
            ToHelpContentType("CT_SUPPORT_FORUM_THREAD"));

  EXPECT_EQ(HelpContentType::kUnknown, ToHelpContentType(""));
  EXPECT_EQ(HelpContentType::kUnknown, ToHelpContentType("CT_BLOB"));
}

// Test the ConvertSearchRequestToJson utility function.
TEST_F(HelpContentProviderTest, ConvertSearchRequestToJson) {
  auto request = SearchRequest::New(u"how do", 10);
  EXPECT_EQ(
      R"({"helpcenter":"chromeos","language":"zh",)"
      R"("max_results":"20","query":"how do"})",
      ConvertSearchRequestToJson("zh", /*is_child_account=*/false, request));
}

// Test the ConvertSearchRequestToJsonWithChildAccount utility function.
TEST_F(HelpContentProviderTest, ConvertSearchRequestToJsonWithChildAccount) {
  auto request = SearchRequest::New(u"how do", 10);
  EXPECT_EQ(
      R"({"helpcenter":"chromeos","language":"zh",)"
      R"("max_results":"30","query":"how do"})",
      ConvertSearchRequestToJson("zh", /*is_child_account=*/true, request));
}

// Test the PopulateSearchResponse utility function with empty json string.
TEST_F(HelpContentProviderTest, PopulateSearchResponseEmpty) {
  auto response = SearchResponse::New();
  PopulateSearchResponseHelper("", response);
  EXPECT_EQ(response->results.size(), 0u);
  EXPECT_EQ(response->total_results, 0u);
}

// Test the PopulateSearchResponse utility function with zero total matches.
TEST_F(HelpContentProviderTest, PopulateSearchResponseZeroMatch) {
  auto response = SearchResponse::New();
  PopulateSearchResponseHelper(R"({totalResults": "0"})", response);
  EXPECT_EQ(response->results.size(), 0u);
  EXPECT_EQ(response->total_results, 0u);
}

// Test the PopulateSearchResponse utility function with two total matches.
// Also verify the urls are always absolute even for relative urls in input.
TEST_F(HelpContentProviderTest, PopulateSearchResponseTwoMatch) {
  auto response = SearchResponse::New();
  PopulateSearchResponseHelper(kFakeResponse, response);
  EXPECT_EQ(response->results.size(), 2u);
  EXPECT_EQ(response->total_results, 2000000u);

  const HelpContentPtr& first = response->results[0];
  EXPECT_EQ(u"fake-title-1", first->title);
  EXPECT_EQ("https://support.google.com/chromebook/fake1?hl=en-gb",
            first->url.spec());
  EXPECT_EQ(HelpContentType::kArticle, first->content_type);

  const HelpContentPtr& second = response->results[1];
  EXPECT_EQ(u"fake-title-2", second->title);
  EXPECT_EQ("https://support.google.com/chromebook/fake2?hl=en-gb",
            second->url.spec());
  EXPECT_EQ(HelpContentType::kForum, second->content_type);
}

// Test Help Contents are fetched Successfully.
TEST_F(HelpContentProviderTest, ResponseSuccessful) {
  test_url_loader_factory_.AddResponse(GetApiUrl(), kFakeResponse,
                                       net::HTTP_OK);

  auto request = SearchRequest::New(u"how do I login", 2);
  InitializeProvider(/*is_child_account=*/false);
  auto response = GetHelpContentsAndWait(std::move(request));
  EXPECT_EQ(response->results.size(), 2u);
  EXPECT_EQ(response->total_results, 2000000u);
}

// Test Help Contents are feched Successfully with a child account.
TEST_F(HelpContentProviderTest, ResponseSuccessfulWithChildAccount) {
  test_url_loader_factory_.AddResponse(GetApiUrl(), kFakeResponse,
                                       net::HTTP_OK);

  auto request = SearchRequest::New(u"how do I login", 2);
  InitializeProvider(/*is_child_account=*/true);
  auto response = GetHelpContentsAndWait(std::move(request));
  EXPECT_EQ(response->results.size(), 1u);
  const HelpContentPtr& first = response->results[0];
  EXPECT_EQ(HelpContentType::kArticle, first->content_type);
  EXPECT_EQ(response->total_results, 2000000u);
}

// Test Help Contents are not fetched due to some error.
TEST_F(HelpContentProviderTest, NetworkError) {
  test_url_loader_factory_.AddResponse(GetApiUrl(), kFakeResponse,
                                       net::HTTP_INTERNAL_SERVER_ERROR);

  auto request = SearchRequest::New(u"how do I login", 2);
  InitializeProvider(/*is_child_account=*/false);
  auto response = GetHelpContentsAndWait(std::move(request));
  EXPECT_EQ(response->results.size(), 0u);
  EXPECT_EQ(response->total_results, 0u);
}

TEST_F(HelpContentProviderTest, ResetReceiverOnBindInterface) {
  // This test simulates a user trying to open a second instant. The receiver
  // should be reset before binding the new receiver. Otherwise we would get a
  // DCHECK error from mojo::Receiver
  InitializeProvider(/*is_child_account=*/false);
  provider_remote_.reset();  // reset the binding done in Setup.
  provider_->BindInterface(provider_remote_.BindNewPipeAndPassReceiver());
  base::RunLoop().RunUntilIdle();
}

}  // namespace feedback
}  // namespace ash
