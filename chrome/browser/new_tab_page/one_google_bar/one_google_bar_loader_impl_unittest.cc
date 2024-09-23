// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/one_google_bar/one_google_bar_loader_impl.h"

#include <optional>

#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/new_tab_page/one_google_bar/one_google_bar_data.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "content/public/test/browser_task_environment.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_test_helper.h"
#endif

using testing::_;
using testing::DoAll;
using testing::Eq;
using testing::IsEmpty;
using testing::SaveArg;
using testing::StartsWith;

namespace {

const char kApplicationLocale[] = "de";

const char kMinimalValidResponse[] = R"json({"update": { "ogb": {
  "html": { "private_do_not_access_or_else_safe_html_wrapped_value": "" },
  "page_hooks": {}
}}})json";

// Returns the value of the "enable_account_consistency" parameter in the
// "X-ChromeConnected" header. The header is expected to be in the format:
//    param1=value1,param2=value2,[...],paramN=valueN
// If the "enable_account_consistency" parameter is not found, returns the empty
// string.
std::string GetEnableAccountConsistencyValue(
    const std::string& chrome_connected_header) {
  base::StringPairs header_params;
  base::SplitStringIntoKeyValuePairs(chrome_connected_header, '=', ',',
                                     &header_params);
  for (const auto& [key, value] : header_params) {
    if (key == "enable_account_consistency")
      return value;
  }
  return std::string();
}

}  // namespace

ACTION_P(Quit, run_loop) {
  run_loop->Quit();
}

class OneGoogleBarLoaderImplTest : public testing::Test {
 public:
  OneGoogleBarLoaderImplTest()
      : OneGoogleBarLoaderImplTest(
            /*account_consistency_mirror_required=*/false) {}

  explicit OneGoogleBarLoaderImplTest(bool account_consistency_mirror_required)
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP),
        test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)),
        account_consistency_mirror_required_(
            account_consistency_mirror_required) {}

  void SetUp() override {
    testing::Test::SetUp();

#if BUILDFLAG(IS_CHROMEOS_LACROS)
    if (!chromeos::LacrosService::Get()) {
      scoped_lacros_test_helper_ =
          std::make_unique<chromeos::ScopedLacrosServiceTestHelper>();
    }
#endif

    one_google_bar_loader_ = std::make_unique<OneGoogleBarLoaderImpl>(
        test_shared_loader_factory_, kApplicationLocale,
        account_consistency_mirror_required_);
  }

  void SetUpResponseWithData(const std::string& response) {
    test_url_loader_factory_.SetInterceptor(base::BindLambdaForTesting(
        [&](const network::ResourceRequest& request) {
          last_request_url_ = request.url;
          last_request_headers_ = request.headers;
        }));
    test_url_loader_factory_.AddResponse(
        one_google_bar_loader_->GetLoadURLForTesting().spec(), response);
  }

  void SetUpResponseWithNetworkError() {
    test_url_loader_factory_.AddResponse(
        one_google_bar_loader_->GetLoadURLForTesting(),
        network::mojom::URLResponseHead::New(), std::string(),
        network::URLLoaderCompletionStatus(net::HTTP_NOT_FOUND));
  }

  OneGoogleBarLoaderImpl* one_google_bar_loader() {
    return one_google_bar_loader_.get();
  }

  GURL last_request_url() { return last_request_url_; }
  net::HttpRequestHeaders last_request_headers() {
    return last_request_headers_;
  }

 private:
  // variations::AppendVariationHeaders requires browser threads.
  content::BrowserTaskEnvironment task_environment_;

  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};

  // Supports JSON decoding in the loader implementation.
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;

  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  bool account_consistency_mirror_required_;

  GURL last_request_url_;
  net::HttpRequestHeaders last_request_headers_;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  std::unique_ptr<chromeos::ScopedLacrosServiceTestHelper>
      scoped_lacros_test_helper_;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  std::unique_ptr<OneGoogleBarLoaderImpl> one_google_bar_loader_;
};

TEST_F(OneGoogleBarLoaderImplTest, RequestUrlContainsLanguage) {
  SetUpResponseWithData(kMinimalValidResponse);

  // Trigger a request.
  base::MockCallback<OneGoogleBarLoader::OneGoogleCallback> callback;
  one_google_bar_loader()->Load(callback.Get());

  base::RunLoop loop;
  EXPECT_CALL(callback, Run(_, _)).WillOnce(Quit(&loop));
  loop.Run();

  // Make sure the request URL contains the "hl=" query param.
  std::string expected_query =
      base::StringPrintf("hl=%s&async=fixed:0", kApplicationLocale);
  EXPECT_EQ(expected_query, last_request_url().query());
}

TEST_F(OneGoogleBarLoaderImplTest, RequestUrlWithAdditionalQueryParams) {
  one_google_bar_loader()->SetAdditionalQueryParams("&test&hl=&async=");
  EXPECT_EQ("test&hl=&async=",
            one_google_bar_loader()->GetLoadURLForTesting().query());
  one_google_bar_loader()->SetAdditionalQueryParams("&test&hl=");
  EXPECT_EQ("test&hl=&async=fixed:0",
            one_google_bar_loader()->GetLoadURLForTesting().query());
  one_google_bar_loader()->SetAdditionalQueryParams("&test&async=");
  EXPECT_EQ(base::StringPrintf("hl=%s&test&async=", kApplicationLocale),
            one_google_bar_loader()->GetLoadURLForTesting().query());
  one_google_bar_loader()->SetAdditionalQueryParams("&test");
  EXPECT_EQ(base::StringPrintf("hl=%s&test&async=fixed:0", kApplicationLocale),
            one_google_bar_loader()->GetLoadURLForTesting().query());
}

TEST_F(OneGoogleBarLoaderImplTest, RequestReturns) {
  SetUpResponseWithData(kMinimalValidResponse);

  base::MockCallback<OneGoogleBarLoader::OneGoogleCallback> callback;
  one_google_bar_loader()->Load(callback.Get());

  std::optional<OneGoogleBarData> data;
  base::RunLoop loop;
  EXPECT_CALL(callback, Run(OneGoogleBarLoader::Status::OK, _))
      .WillOnce(DoAll(SaveArg<1>(&data), Quit(&loop)));
  loop.Run();

  EXPECT_TRUE(data.has_value());
}

TEST_F(OneGoogleBarLoaderImplTest, HandlesResponsePreamble) {
  // The reponse may contain a ")]}'" prefix. The loader should ignore that
  // during parsing.
  SetUpResponseWithData(std::string(")]}'") + kMinimalValidResponse);

  base::MockCallback<OneGoogleBarLoader::OneGoogleCallback> callback;
  one_google_bar_loader()->Load(callback.Get());

  std::optional<OneGoogleBarData> data;
  base::RunLoop loop;
  EXPECT_CALL(callback, Run(OneGoogleBarLoader::Status::OK, _))
      .WillOnce(DoAll(SaveArg<1>(&data), Quit(&loop)));
  loop.Run();

  EXPECT_TRUE(data.has_value());
}

TEST_F(OneGoogleBarLoaderImplTest, ParsesFullResponse) {
  SetUpResponseWithData(R"json({"update": { "ogb": {
    "html": {
      "private_do_not_access_or_else_safe_html_wrapped_value": "bar_html_value"
    },
    "page_hooks": {
      "in_head_script": {
        "private_do_not_access_or_else_safe_script_wrapped_value":
          "in_head_script_value"
      },
      "in_head_style": {
        "private_do_not_access_or_else_safe_style_sheet_wrapped_value":
          "in_head_style_value"
      },
      "after_bar_script": {
        "private_do_not_access_or_else_safe_script_wrapped_value":
          "after_bar_script_value"
      },
      "end_of_body_html": {
        "private_do_not_access_or_else_safe_html_wrapped_value":
          "end_of_body_html_value"
      },
      "end_of_body_script": {
        "private_do_not_access_or_else_safe_script_wrapped_value":
          "end_of_body_script_value"
      }
    }
  }}})json");

  base::MockCallback<OneGoogleBarLoader::OneGoogleCallback> callback;
  one_google_bar_loader()->Load(callback.Get());

  std::optional<OneGoogleBarData> data;
  base::RunLoop loop;
  EXPECT_CALL(callback, Run(OneGoogleBarLoader::Status::OK, _))
      .WillOnce(DoAll(SaveArg<1>(&data), Quit(&loop)));
  loop.Run();

  ASSERT_TRUE(data.has_value());
  EXPECT_THAT(data->bar_html, Eq("bar_html_value"));
  EXPECT_THAT(data->in_head_script, Eq("in_head_script_value"));
  EXPECT_THAT(data->in_head_style, Eq("in_head_style_value"));
  EXPECT_THAT(data->after_bar_script, Eq("after_bar_script_value"));
  EXPECT_THAT(data->end_of_body_html, Eq("end_of_body_html_value"));
  EXPECT_THAT(data->end_of_body_script, Eq("end_of_body_script_value"));
}

TEST_F(OneGoogleBarLoaderImplTest, CoalescesMultipleRequests) {
  SetUpResponseWithData(kMinimalValidResponse);

  // Trigger two requests.
  base::MockCallback<OneGoogleBarLoader::OneGoogleCallback> first_callback;
  one_google_bar_loader()->Load(first_callback.Get());
  base::MockCallback<OneGoogleBarLoader::OneGoogleCallback> second_callback;
  one_google_bar_loader()->Load(second_callback.Get());

  // Make sure that a single response causes both callbacks to be called.
  std::optional<OneGoogleBarData> first_data;
  std::optional<OneGoogleBarData> second_data;

  base::RunLoop loop;
  EXPECT_CALL(first_callback, Run(OneGoogleBarLoader::Status::OK, _))
      .WillOnce(SaveArg<1>(&first_data));
  EXPECT_CALL(second_callback, Run(OneGoogleBarLoader::Status::OK, _))
      .WillOnce(DoAll(SaveArg<1>(&second_data), Quit(&loop)));
  loop.Run();

  // Ensure that both requests received a response.
  EXPECT_TRUE(first_data.has_value());
  EXPECT_TRUE(second_data.has_value());
}

TEST_F(OneGoogleBarLoaderImplTest, NetworkErrorIsTransient) {
  SetUpResponseWithNetworkError();

  base::MockCallback<OneGoogleBarLoader::OneGoogleCallback> callback;
  one_google_bar_loader()->Load(callback.Get());

  base::RunLoop loop;
  EXPECT_CALL(callback, Run(OneGoogleBarLoader::Status::TRANSIENT_ERROR,
                            Eq(std::nullopt)))
      .WillOnce(Quit(&loop));
  loop.Run();
}

TEST_F(OneGoogleBarLoaderImplTest, InvalidJsonErrorIsFatal) {
  SetUpResponseWithData(kMinimalValidResponse + std::string(")"));

  base::MockCallback<OneGoogleBarLoader::OneGoogleCallback> callback;
  one_google_bar_loader()->Load(callback.Get());

  base::RunLoop loop;
  EXPECT_CALL(callback,
              Run(OneGoogleBarLoader::Status::FATAL_ERROR, Eq(std::nullopt)))
      .WillOnce(Quit(&loop));
  loop.Run();
}

TEST_F(OneGoogleBarLoaderImplTest, IncompleteJsonErrorIsFatal) {
  SetUpResponseWithData(R"json({"update": { "ogb": {
  "html": {},
  "page_hooks": {}
}}})json");

  base::MockCallback<OneGoogleBarLoader::OneGoogleCallback> callback;
  one_google_bar_loader()->Load(callback.Get());

  base::RunLoop loop;
  EXPECT_CALL(callback,
              Run(OneGoogleBarLoader::Status::FATAL_ERROR, Eq(std::nullopt)))
      .WillOnce(Quit(&loop));
  loop.Run();
}

TEST_F(OneGoogleBarLoaderImplTest, MirrorAccountConsistencyNotRequired) {
  SetUpResponseWithData(kMinimalValidResponse);

  // Trigger a request.
  base::MockCallback<OneGoogleBarLoader::OneGoogleCallback> callback;
  one_google_bar_loader()->Load(callback.Get());

  base::RunLoop loop;
  EXPECT_CALL(callback, Run(_, _)).WillOnce(Quit(&loop));
  loop.Run();

  // On not Chrome OS, the X-Chrome-Connected header must not be present.
  bool check_x_chrome_connected_header = false;
#if BUILDFLAG(IS_CHROMEOS)
  check_x_chrome_connected_header = true;
#endif

  if (check_x_chrome_connected_header) {
    // On Chrome OS, X-Chrome-Connected header is present, but
    // enable_account_consistency is set to false.
    EXPECT_THAT(
        last_request_headers().GetHeader(signin::kChromeConnectedHeader),
        testing::Optional(
            testing::ResultOf(&GetEnableAccountConsistencyValue, "false")));
    // mode = PROFILE_MODE_DEFAULT
  } else {
    // On not Chrome OS, the X-Chrome-Connected header must not be present.
    EXPECT_FALSE(
        last_request_headers().HasHeader(signin::kChromeConnectedHeader));
  }
}

class OneGoogleBarLoaderImplWithMirrorAccountConsistencyTest
    : public OneGoogleBarLoaderImplTest {
 public:
  OneGoogleBarLoaderImplWithMirrorAccountConsistencyTest()
      : OneGoogleBarLoaderImplTest(true) {}
};

TEST_F(OneGoogleBarLoaderImplWithMirrorAccountConsistencyTest,
       MirrorAccountConsistencyRequired) {
  SetUpResponseWithData(kMinimalValidResponse);

  // Trigger a request.
  base::MockCallback<OneGoogleBarLoader::OneGoogleCallback> callback;
  one_google_bar_loader()->Load(callback.Get());

  base::RunLoop loop;
  EXPECT_CALL(callback, Run(_, _)).WillOnce(Quit(&loop));
  loop.Run();

  // On not Chrome OS, the X-Chrome-Connected header must not be present.
  bool check_x_chrome_connected_header = false;
#if BUILDFLAG(IS_CHROMEOS)
  check_x_chrome_connected_header = true;
#endif

  // Make sure mirror account consistency is requested.
  if (check_x_chrome_connected_header) {
    // On Chrome OS, X-Chrome-Connected header is present, and
    // enable_account_consistency is set to true.
    // mode = PROFILE_MODE_INCOGNITO_DISABLED |
    // PROFILE_MODE_ADD_ACCOUNT_DISABLED
    EXPECT_THAT(
        last_request_headers().GetHeader(signin::kChromeConnectedHeader),
        testing::Optional(
            testing::ResultOf(&GetEnableAccountConsistencyValue, "true")));
  } else {
    // This is not a valid case (mirror account consistency can only be required
    // on Chrome OS). This ensures in this case nothing happens.
    EXPECT_FALSE(
        last_request_headers().HasHeader(signin::kChromeConnectedHeader));
  }
}

TEST_F(OneGoogleBarLoaderImplTest, ParsesLanguageCode) {
  SetUpResponseWithData(R"json({"update": { "language_code": "en-US", "ogb": {
  "html": { "private_do_not_access_or_else_safe_html_wrapped_value": "" },
  "page_hooks": {}
  }}})json");

  base::MockCallback<OneGoogleBarLoader::OneGoogleCallback> callback;
  one_google_bar_loader()->Load(callback.Get());

  std::optional<OneGoogleBarData> data;
  base::RunLoop loop;
  EXPECT_CALL(callback, Run(OneGoogleBarLoader::Status::OK, _))
      .WillOnce(DoAll(SaveArg<1>(&data), Quit(&loop)));
  loop.Run();

  ASSERT_TRUE(data.has_value());
  EXPECT_THAT(data->language_code, Eq("en-US"));
}
