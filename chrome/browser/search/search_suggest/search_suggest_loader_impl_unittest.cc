// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/search_suggest/search_suggest_loader_impl.h"

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind_test_util.h"
#include "base/test/mock_callback.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/search/search_suggest/search_suggest_data.h"
#include "content/public/test/browser_task_environment.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Eq;
using testing::IsEmpty;
using testing::SaveArg;
using testing::StartsWith;

namespace {

const char kApplicationLocale[] = "us";

const char kMinimalValidResponseNoSuggestions[] =
    R"json({"update": { "query_suggestions": { "impression_cap_expire_time_ms":
     1, "request_freeze_time_ms": 2, "max_impressions": 3}}})json";

const char kMinimalValidResponseWithSuggestions[] =
    R"json({"update": { "query_suggestions": {"query_suggestions_with_html":
    "<div></div>", "script": "<script></script>","impression_cap_expire_time_ms"
    : 1, "request_freeze_time_ms": 2,"max_impressions": 3}}})json";

}  // namespace

ACTION_P(Quit, run_loop) {
  run_loop->Quit();
}

class SearchSuggestLoaderImplTest : public testing::Test {
 public:
  SearchSuggestLoaderImplTest()
      : SearchSuggestLoaderImplTest(
            /*account_consistency_mirror_required=*/false) {}

  explicit SearchSuggestLoaderImplTest(bool account_consistency_mirror_required)
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP),
        test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {}

  void SetUp() override {
    testing::Test::SetUp();

    search_suggest_loader_ = std::make_unique<SearchSuggestLoaderImpl>(
        test_shared_loader_factory_, kApplicationLocale);
  }

  void SetUpResponseWithData(const std::string& response) {
    test_url_loader_factory_.SetInterceptor(base::BindLambdaForTesting(
        [&](const network::ResourceRequest& request) {
          last_request_url_ = request.url;
          last_request_headers_ = request.headers;
        }));
    test_url_loader_factory_.AddResponse(
        search_suggest_loader_->GetLoadURLForTesting().spec(), response);
  }

  void SetUpResponseWithNetworkError() {
    test_url_loader_factory_.AddResponse(
        search_suggest_loader_->GetLoadURLForTesting(),
        network::mojom::URLResponseHead::New(), std::string(),
        network::URLLoaderCompletionStatus(net::HTTP_NOT_FOUND));
  }

  SearchSuggestLoaderImpl* search_suggest_loader() {
    return search_suggest_loader_.get();
  }

  GURL last_request_url() { return last_request_url_; }
  net::HttpRequestHeaders last_request_headers() {
    return last_request_headers_;
  }

 private:
  // variations::AppendVariationHeaders requires browser threads.
  content::BrowserTaskEnvironment task_environment_;

  // Supports JSON parsing in the loader impl.
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;

  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;

  GURL last_request_url_;
  net::HttpRequestHeaders last_request_headers_;

  std::unique_ptr<SearchSuggestLoaderImpl> search_suggest_loader_;
};

TEST_F(SearchSuggestLoaderImplTest, RequestReturns) {
  SetUpResponseWithData(kMinimalValidResponseWithSuggestions);

  base::MockCallback<SearchSuggestLoader::SearchSuggestionsCallback> callback;
  std::string blocklist;
  search_suggest_loader()->Load(blocklist, callback.Get());

  base::Optional<SearchSuggestData> data;
  base::RunLoop loop;
  EXPECT_CALL(callback,
              Run(SearchSuggestLoader::Status::OK_WITH_SUGGESTIONS, _))
      .WillOnce(DoAll(SaveArg<1>(&data), Quit(&loop)));
  loop.Run();

  EXPECT_TRUE(data.has_value());
}

TEST_F(SearchSuggestLoaderImplTest, HandlesResponsePreamble) {
  // The response may contain a ")]}'" prefix. The loader should ignore that
  // during parsing.
  SetUpResponseWithData(std::string(")]}'") +
                        kMinimalValidResponseWithSuggestions);

  base::MockCallback<SearchSuggestLoader::SearchSuggestionsCallback> callback;
  std::string blocklist;
  search_suggest_loader()->Load(blocklist, callback.Get());

  base::Optional<SearchSuggestData> data;
  base::RunLoop loop;
  EXPECT_CALL(callback,
              Run(SearchSuggestLoader::Status::OK_WITH_SUGGESTIONS, _))
      .WillOnce(DoAll(SaveArg<1>(&data), Quit(&loop)));
  loop.Run();

  EXPECT_TRUE(data.has_value());
}

TEST_F(SearchSuggestLoaderImplTest, ParsesFullResponse) {
  SetUpResponseWithData(kMinimalValidResponseWithSuggestions);

  base::MockCallback<SearchSuggestLoader::SearchSuggestionsCallback> callback;
  std::string blocklist;
  search_suggest_loader()->Load(blocklist, callback.Get());

  base::Optional<SearchSuggestData> data;
  base::RunLoop loop;
  EXPECT_CALL(callback,
              Run(SearchSuggestLoader::Status::OK_WITH_SUGGESTIONS, _))
      .WillOnce(DoAll(SaveArg<1>(&data), Quit(&loop)));
  loop.Run();

  ASSERT_TRUE(data.has_value());
  EXPECT_EQ("<div></div>", data->suggestions_html);
  EXPECT_EQ("<script></script>", data->end_of_body_script);
  EXPECT_EQ(1, data->impression_cap_expire_time_ms);
  EXPECT_EQ(2, data->request_freeze_time_ms);
  EXPECT_EQ(3, data->max_impressions);
}

TEST_F(SearchSuggestLoaderImplTest, ParsesValidResponseWithNoSuggestions) {
  SetUpResponseWithData(kMinimalValidResponseNoSuggestions);

  base::MockCallback<SearchSuggestLoader::SearchSuggestionsCallback> callback;
  std::string blocklist;
  search_suggest_loader()->Load(blocklist, callback.Get());

  base::Optional<SearchSuggestData> data;
  base::RunLoop loop;
  EXPECT_CALL(callback,
              Run(SearchSuggestLoader::Status::OK_WITHOUT_SUGGESTIONS, _))
      .WillOnce(DoAll(SaveArg<1>(&data), Quit(&loop)));
  loop.Run();

  ASSERT_TRUE(data.has_value());
  EXPECT_EQ(std::string(), data->suggestions_html);
  EXPECT_EQ(std::string(), data->end_of_body_script);
  EXPECT_EQ(1, data->impression_cap_expire_time_ms);
  EXPECT_EQ(2, data->request_freeze_time_ms);
  EXPECT_EQ(3, data->max_impressions);
}

TEST_F(SearchSuggestLoaderImplTest, CoalescesMultipleRequests) {
  SetUpResponseWithData(kMinimalValidResponseWithSuggestions);

  // Trigger two requests.
  base::MockCallback<SearchSuggestLoader::SearchSuggestionsCallback>
      first_callback;
  std::string blocklist;
  search_suggest_loader()->Load(blocklist, first_callback.Get());
  base::MockCallback<SearchSuggestLoader::SearchSuggestionsCallback>
      second_callback;
  search_suggest_loader()->Load(blocklist, second_callback.Get());

  // Make sure that a single response causes both callbacks to be called.
  base::Optional<SearchSuggestData> first_data;
  base::Optional<SearchSuggestData> second_data;

  base::RunLoop loop;
  EXPECT_CALL(first_callback,
              Run(SearchSuggestLoader::Status::OK_WITH_SUGGESTIONS, _))
      .WillOnce(SaveArg<1>(&first_data));
  EXPECT_CALL(second_callback,
              Run(SearchSuggestLoader::Status::OK_WITH_SUGGESTIONS, _))
      .WillOnce(DoAll(SaveArg<1>(&second_data), Quit(&loop)));
  loop.Run();

  // Ensure that both requests received a response.
  EXPECT_TRUE(first_data.has_value());
  EXPECT_TRUE(second_data.has_value());
}

TEST_F(SearchSuggestLoaderImplTest, NetworkErrorIsTransient) {
  SetUpResponseWithNetworkError();

  base::MockCallback<SearchSuggestLoader::SearchSuggestionsCallback> callback;
  std::string blocklist;
  search_suggest_loader()->Load(blocklist, callback.Get());

  base::RunLoop loop;
  EXPECT_CALL(callback, Run(SearchSuggestLoader::Status::TRANSIENT_ERROR,
                            Eq(base::nullopt)))
      .WillOnce(Quit(&loop));
  loop.Run();
}

// Flaky, see https://crbug.com/923953.
TEST_F(SearchSuggestLoaderImplTest, DISABLED_InvalidJsonErrorIsFatal) {
  SetUpResponseWithData(kMinimalValidResponseWithSuggestions +
                        std::string(")"));

  base::MockCallback<SearchSuggestLoader::SearchSuggestionsCallback> callback;
  std::string blocklist;
  search_suggest_loader()->Load(blocklist, callback.Get());

  base::RunLoop loop;
  EXPECT_CALL(callback,
              Run(SearchSuggestLoader::Status::FATAL_ERROR, Eq(base::nullopt)))
      .WillOnce(Quit(&loop));
  loop.Run();
}

TEST_F(SearchSuggestLoaderImplTest, IncompleteJsonErrorIsFatal) {
  SetUpResponseWithData(R"json({"update": {}})json");

  base::MockCallback<SearchSuggestLoader::SearchSuggestionsCallback> callback;
  std::string blocklist;
  search_suggest_loader()->Load(blocklist, callback.Get());

  base::RunLoop loop;
  EXPECT_CALL(callback,
              Run(SearchSuggestLoader::Status::FATAL_ERROR, Eq(base::nullopt)))
      .WillOnce(Quit(&loop));
  loop.Run();
}
