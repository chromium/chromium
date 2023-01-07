// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/oauth2/http_exchange.h"

#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash {
namespace {

// The structure holding objects/variables defining HTTP response. The default
// constructor creates correct response returning empty JSON object.
struct HttpExchangeDefinition {
  explicit HttpExchangeDefinition(
      net::HttpStatusCode status = net::HttpStatusCode::HTTP_OK)
      : url("http://a.b/c"),
        response_head(network::CreateURLResponseHead(status)),
        response_content("{}") {
    response_head->headers->SetHeader("Content-Type", "application/json");
  }
  std::string url;
  network::mojom::URLResponseHeadPtr response_head;
  std::string response_content;
  network::URLLoaderCompletionStatus compl_status;
};

class PrintingOAuth2HttpExchangeTest : public testing::Test {
 public:
  PrintingOAuth2HttpExchangeTest()
      : url_loader_factory_(),
        http_exchange_(url_loader_factory_.GetSafeWeakWrapper()) {}
  ~PrintingOAuth2HttpExchangeTest() override {}
  // Helper method calling http_exchange_.Exchange("GET", ...).
  printing::oauth2::StatusCode Exchange(
      HttpExchangeDefinition def,
      net::HttpStatusCode success_code = net::HttpStatusCode::HTTP_OK,
      net::HttpStatusCode error_code = net::HttpStatusCode::HTTP_BAD_REQUEST) {
    url_loader_factory_.AddResponse(GURL(def.url), std::move(def.response_head),
                                    def.response_content, def.compl_status);
    http_exchange_.Exchange(
        "GET", GURL(def.url), printing::oauth2::ContentFormat::kEmpty,
        static_cast<int>(success_code), static_cast<int>(error_code),
        PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS,
        base::BindOnce(&PrintingOAuth2HttpExchangeTest::ExchangeCallback,
                       base::Unretained(this)));
    task_environment_.RunUntilIdle();
    return callback_status_;
  }
  // This callback is used by the method above.
  void ExchangeCallback(printing::oauth2::StatusCode status) {
    callback_status_ = status;
  }

 protected:
  network::TestURLLoaderFactory url_loader_factory_;
  printing::oauth2::HttpExchange http_exchange_;
  base::test::TaskEnvironment task_environment_;
  printing::oauth2::StatusCode callback_status_;
};

constexpr char kExampleContent[] = R"({
  "arr1": ["v1", "v2"],
  "arr2": ["x1", 12, "x2"],
  "int": 123,
  "url1": "http://a:123/b",
  "url2": "https://abc.de/12",
  "str1": "123",
  "str2": "abc",
  "emptyStr": "" })";

TEST_F(PrintingOAuth2HttpExchangeTest, ConnectionError) {
  HttpExchangeDefinition def;
  def.compl_status.error_code = -100;
  auto status = Exchange(std::move(def));
  EXPECT_EQ(status, printing::oauth2::StatusCode::kConnectionError);
  EXPECT_FALSE(http_exchange_.GetErrorMessage().empty());
  EXPECT_EQ(http_exchange_.GetHttpStatus(), 0);
}

TEST_F(PrintingOAuth2HttpExchangeTest, ServerError) {
  HttpExchangeDefinition def(net::HttpStatusCode::HTTP_INTERNAL_SERVER_ERROR);
  auto status = Exchange(std::move(def));
  EXPECT_EQ(status, printing::oauth2::StatusCode::kServerError);
  EXPECT_FALSE(http_exchange_.GetErrorMessage().empty());
  EXPECT_EQ(http_exchange_.GetHttpStatus(), 500 /*HTTP_INTERNAL_SERVER_ERROR*/);
}

TEST_F(PrintingOAuth2HttpExchangeTest, ServerTemporarilyUnavailable) {
  HttpExchangeDefinition def(net::HttpStatusCode::HTTP_SERVICE_UNAVAILABLE);
  auto status = Exchange(std::move(def));
  EXPECT_EQ(status,
            printing::oauth2::StatusCode::kServerTemporarilyUnavailable);
  EXPECT_FALSE(http_exchange_.GetErrorMessage().empty());
  EXPECT_EQ(http_exchange_.GetHttpStatus(), 503 /*HTTP_SERVICE_UNAVAILABLE*/);
}

TEST_F(PrintingOAuth2HttpExchangeTest, InvalidResponseUnknownStatus) {
  HttpExchangeDefinition def(net::HttpStatusCode::HTTP_NO_CONTENT);
  auto status = Exchange(std::move(def));
  EXPECT_EQ(status, printing::oauth2::StatusCode::kInvalidResponse);
  EXPECT_FALSE(http_exchange_.GetErrorMessage().empty());
  EXPECT_EQ(http_exchange_.GetHttpStatus(), 204 /*HTTP_NO_CONTENT*/);
}

TEST_F(PrintingOAuth2HttpExchangeTest, InvalidResponseNoJson) {
  HttpExchangeDefinition def;
  def.response_content = "abcdef";
  auto status = Exchange(std::move(def));
  EXPECT_EQ(status, printing::oauth2::StatusCode::kInvalidResponse);
  EXPECT_FALSE(http_exchange_.GetErrorMessage().empty());
  EXPECT_EQ(http_exchange_.GetHttpStatus(), 200 /*HTTP_OK*/);
}

TEST_F(PrintingOAuth2HttpExchangeTest, InvalidResponseNoErrorField) {
  HttpExchangeDefinition def(net::HttpStatusCode::HTTP_BAD_REQUEST);
  auto status = Exchange(std::move(def));
  EXPECT_EQ(status, printing::oauth2::StatusCode::kInvalidResponse);
  EXPECT_FALSE(http_exchange_.GetErrorMessage().empty());
  EXPECT_EQ(http_exchange_.GetHttpStatus(), 400 /*HTTP_BAD_REQUEST*/);
}

TEST_F(PrintingOAuth2HttpExchangeTest, InvalidAccessToken) {
  HttpExchangeDefinition def(net::HttpStatusCode::HTTP_BAD_REQUEST);
  def.response_content = "{\"error\": \"invalid_grant\" }";
  auto status = Exchange(std::move(def));
  EXPECT_EQ(status, printing::oauth2::StatusCode::kInvalidAccessToken);
  EXPECT_FALSE(http_exchange_.GetErrorMessage().empty());
  EXPECT_EQ(http_exchange_.GetHttpStatus(), 400 /*HTTP_BAD_REQUEST*/);
}

TEST_F(PrintingOAuth2HttpExchangeTest, AccessDenied) {
  HttpExchangeDefinition def(net::HttpStatusCode::HTTP_BAD_REQUEST);
  def.response_content = "{\"error\": \"whatever\" }";
  auto status = Exchange(std::move(def));
  EXPECT_EQ(status, printing::oauth2::StatusCode::kAccessDenied);
  EXPECT_FALSE(http_exchange_.GetErrorMessage().empty());
  EXPECT_EQ(http_exchange_.GetHttpStatus(), 400 /*HTTP_BAD_REQUEST*/);
}

TEST_F(PrintingOAuth2HttpExchangeTest, CorrectEmptyResponse) {
  HttpExchangeDefinition def;
  auto status = Exchange(std::move(def));
  EXPECT_EQ(status, printing::oauth2::StatusCode::kOK);
  EXPECT_TRUE(http_exchange_.GetErrorMessage().empty());
  EXPECT_EQ(http_exchange_.GetHttpStatus(), 200 /*HTTP_OK*/);
}

TEST_F(PrintingOAuth2HttpExchangeTest, NonStandardHttpStatus1) {
  HttpExchangeDefinition def;
  auto status = Exchange(std::move(def), net::HttpStatusCode::HTTP_NO_CONTENT);
  EXPECT_EQ(status, printing::oauth2::StatusCode::kInvalidResponse);
  EXPECT_FALSE(http_exchange_.GetErrorMessage().empty());
  EXPECT_EQ(http_exchange_.GetHttpStatus(), 200 /*HTTP_OK*/);
}

TEST_F(PrintingOAuth2HttpExchangeTest, NonStandardHttpStatus2) {
  HttpExchangeDefinition def(net::HttpStatusCode::HTTP_NO_CONTENT);
  auto status = Exchange(std::move(def), net::HttpStatusCode::HTTP_NO_CONTENT);
  EXPECT_EQ(status, printing::oauth2::StatusCode::kOK);
  EXPECT_TRUE(http_exchange_.GetErrorMessage().empty());
  EXPECT_EQ(http_exchange_.GetHttpStatus(), 204 /*HTTP_NO_CONTENT*/);
}

TEST_F(PrintingOAuth2HttpExchangeTest, NonStandardHttpStatus3) {
  HttpExchangeDefinition def;
  def.response_content = "{\"error\": \"whatever\" }";
  auto status = Exchange(std::move(def), net::HttpStatusCode::HTTP_NO_CONTENT,
                         net::HttpStatusCode::HTTP_OK);
  // Obtained HTTP status (200) means error message in this case.
  EXPECT_EQ(status, printing::oauth2::StatusCode::kAccessDenied);
  EXPECT_FALSE(http_exchange_.GetErrorMessage().empty());
  EXPECT_EQ(http_exchange_.GetHttpStatus(), 200 /*HTTP_OK*/);
}

TEST_F(PrintingOAuth2HttpExchangeTest, Clear) {
  HttpExchangeDefinition def(net::HttpStatusCode::HTTP_BAD_REQUEST);
  def.response_content = "{\"aaa\": \"bbb\" }";
  auto status = Exchange(std::move(def));
  // Obtained HTTP status (400) means error message. It is invalid because the
  // field "error" is missing.
  EXPECT_EQ(status, printing::oauth2::StatusCode::kInvalidResponse);
  EXPECT_FALSE(http_exchange_.GetErrorMessage().empty());
  EXPECT_EQ(http_exchange_.GetHttpStatus(), 400 /*HTTP_BAD_REQUEST*/);
  std::string aaa;
  EXPECT_TRUE(http_exchange_.ParamStringGet("aaa", true, &aaa));
  EXPECT_EQ(aaa, "bbb");
  http_exchange_.Clear();
  EXPECT_TRUE(http_exchange_.GetErrorMessage().empty());
  EXPECT_EQ(http_exchange_.GetHttpStatus(), 0);
  aaa.clear();
  EXPECT_FALSE(http_exchange_.ParamStringGet("aaa", true, &aaa));
  EXPECT_EQ(aaa, "");
}

TEST_F(PrintingOAuth2HttpExchangeTest, MissingResponseParam) {
  HttpExchangeDefinition def;
  def.response_content = kExampleContent;
  auto status = Exchange(std::move(def));
  EXPECT_EQ(status, printing::oauth2::StatusCode::kOK);
  // Missing parameter is an error <=> `required` == true
  EXPECT_FALSE(http_exchange_.ParamArrayStringContains("miss1", true, "123"));
  EXPECT_TRUE(http_exchange_.ParamArrayStringContains("miss1", false, "123"));
  EXPECT_FALSE(http_exchange_.ParamArrayStringEquals("miss2", true, {}));
  EXPECT_TRUE(http_exchange_.ParamArrayStringEquals("miss2", false, {}));
  std::string value = "abc123";
  EXPECT_FALSE(http_exchange_.ParamStringGet("miss3", true, &value));
  EXPECT_TRUE(http_exchange_.ParamStringGet("miss3", false, &value));
  EXPECT_EQ(value, "abc123");
  EXPECT_FALSE(http_exchange_.ParamStringEquals("miss4", true, "123"));
  EXPECT_TRUE(http_exchange_.ParamStringEquals("miss4", false, "123"));
  GURL url("http://abc123/d");
  EXPECT_FALSE(http_exchange_.ParamURLGet("miss5", true, &url));
  EXPECT_TRUE(http_exchange_.ParamURLGet("miss5", false, &url));
  EXPECT_EQ(url.spec(), "http://abc123/d");
  EXPECT_FALSE(http_exchange_.ParamURLEquals("miss6", true, url));
  EXPECT_TRUE(http_exchange_.ParamURLEquals("miss6", false, url));
}

class ParamRequired : public PrintingOAuth2HttpExchangeTest,
                      public testing::WithParamInterface<bool> {};

TEST_P(ParamRequired, ResponseParamTypeMismatch) {
  HttpExchangeDefinition def;
  def.response_content = kExampleContent;
  auto status = Exchange(std::move(def));
  EXPECT_EQ(status, printing::oauth2::StatusCode::kOK);
  // The type mismatch is always an error, even if `required` == false.
  const bool req = GetParam();
  EXPECT_FALSE(http_exchange_.ParamArrayStringContains("int", req, "123"));
  EXPECT_FALSE(http_exchange_.ParamArrayStringContains("str1", req, "123"));
  EXPECT_FALSE(http_exchange_.ParamArrayStringEquals("int", req, {"123"}));
  EXPECT_FALSE(http_exchange_.ParamArrayStringEquals("str1", req, {"123"}));
  std::string value = "abc123";
  EXPECT_FALSE(http_exchange_.ParamStringGet("arr1", req, &value));
  EXPECT_FALSE(http_exchange_.ParamStringGet("int", req, &value));
  EXPECT_EQ(value, "abc123");
  EXPECT_FALSE(http_exchange_.ParamStringEquals("arr1", req, "v0"));
  EXPECT_FALSE(http_exchange_.ParamStringEquals("int", req, "v0"));
  GURL url("http://abc123/d");
  EXPECT_FALSE(http_exchange_.ParamURLGet("arr2", req, &url));
  EXPECT_FALSE(http_exchange_.ParamURLGet("str2", req, &url));
  EXPECT_EQ(url.spec(), "http://abc123/d");
  EXPECT_FALSE(http_exchange_.ParamURLEquals("arr1", req, url));
  EXPECT_FALSE(http_exchange_.ParamURLEquals("str1", req, url));
}

TEST_P(ParamRequired, ParamArrayStringContains) {
  HttpExchangeDefinition def;
  def.response_content = kExampleContent;
  auto status = Exchange(std::move(def));
  EXPECT_EQ(status, printing::oauth2::StatusCode::kOK);
  // If the param exists, these methods work the same way for any `required`.
  const bool req = GetParam();
  EXPECT_TRUE(http_exchange_.ParamArrayStringContains("arr1", req, "v1"));
  EXPECT_FALSE(http_exchange_.ParamArrayStringContains("arr1", req, "v0"));
  EXPECT_TRUE(http_exchange_.ParamArrayStringContains("arr2", req, "x2"));
  EXPECT_FALSE(http_exchange_.ParamArrayStringContains("arr2", req, "12"));
}

TEST_P(ParamRequired, ParamArrayStringEquals) {
  HttpExchangeDefinition def;
  def.response_content = kExampleContent;
  auto status = Exchange(std::move(def));
  EXPECT_EQ(status, printing::oauth2::StatusCode::kOK);
  // If the param exists, these methods work the same way for any `required`.
  const bool req = GetParam();
  EXPECT_FALSE(
      http_exchange_.ParamArrayStringEquals("arr1", req, {"v2", "v1"}));
  EXPECT_TRUE(http_exchange_.ParamArrayStringEquals("arr1", req, {"v1", "v2"}));
  EXPECT_FALSE(
      http_exchange_.ParamArrayStringEquals("arr2", req, {"x1", "12", "x2"}));
}

TEST_P(ParamRequired, ParamStringGet) {
  HttpExchangeDefinition def;
  def.response_content = kExampleContent;
  auto status = Exchange(std::move(def));
  EXPECT_EQ(status, printing::oauth2::StatusCode::kOK);
  // If the param exists and is non-empty, this method works the same way for
  // any `required`.
  const bool req = GetParam();
  std::string value;
  EXPECT_TRUE(http_exchange_.ParamStringGet("str1", req, &value));
  EXPECT_EQ(value, "123");
  EXPECT_TRUE(http_exchange_.ParamStringGet("str2", req, nullptr));
  // Empty string is not allowed when the parameter is required.
  const bool out = http_exchange_.ParamStringGet("emptyStr", req, &value);
  if (req) {
    EXPECT_FALSE(out);
    EXPECT_EQ(value, "123");  // the previous value was preserved
  } else {
    EXPECT_TRUE(out);
    EXPECT_EQ(value, "");
  }
}

TEST_P(ParamRequired, ParamStringEquals) {
  HttpExchangeDefinition def;
  def.response_content = kExampleContent;
  auto status = Exchange(std::move(def));
  EXPECT_EQ(status, printing::oauth2::StatusCode::kOK);
  // If the param exists, these methods work the same way for any `required`.
  EXPECT_FALSE(http_exchange_.ParamStringEquals("str1", GetParam(), "abc"));
  EXPECT_TRUE(http_exchange_.ParamStringEquals("str1", GetParam(), "123"));
}

TEST_P(ParamRequired, ParamURLGet) {
  HttpExchangeDefinition def;
  def.response_content = kExampleContent;
  auto status = Exchange(std::move(def));
  EXPECT_EQ(status, printing::oauth2::StatusCode::kOK);
  // If the param exists, these methods work the same way for any `required`.
  GURL value;
  EXPECT_TRUE(http_exchange_.ParamURLGet("url2", GetParam(), &value));
  EXPECT_EQ(value.spec(), "https://abc.de/12");
  EXPECT_TRUE(http_exchange_.ParamURLGet("url2", GetParam(), nullptr));
  EXPECT_FALSE(http_exchange_.ParamURLGet("url1", GetParam(), &value));
}

TEST_P(ParamRequired, ParamURLEquals) {
  HttpExchangeDefinition def;
  def.response_content = kExampleContent;
  auto status = Exchange(std::move(def));
  EXPECT_EQ(status, printing::oauth2::StatusCode::kOK);
  // If the param exists, these methods work the same way for any `required`.
  const bool req = GetParam();
  EXPECT_TRUE(
      http_exchange_.ParamURLEquals("url2", req, GURL("https://abc.de/12")));
  EXPECT_FALSE(
      http_exchange_.ParamURLEquals("url2", req, GURL("https://abc.de")));
  EXPECT_TRUE(
      http_exchange_.ParamURLEquals("url1", req, GURL("http://a:123/b")));
}

INSTANTIATE_TEST_SUITE_P(PrintingOAuth2HttpExchangeWithParamTest,
                         ParamRequired,
                         testing::Values(true, false));

}  // namespace
}  // namespace ash
