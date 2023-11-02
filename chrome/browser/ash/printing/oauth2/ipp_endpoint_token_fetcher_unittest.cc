// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/oauth2/ipp_endpoint_token_fetcher.h"

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/values.h"
#include "chrome/browser/ash/printing/oauth2/constants.h"
#include "chrome/browser/ash/printing/oauth2/status_code.h"
#include "chrome/browser/ash/printing/oauth2/test_authorization_server.h"
#include "net/http/http_status_code.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash {
namespace printing {
namespace oauth2 {
namespace {

class PrintingOAuth2IppEndpointTokenFetcherTest : public testing::Test {
 protected:
  // Initialize the `session_` field.
  void CreateSession(std::string&& endpoint_uri,
                     base::flat_set<std::string>&& scope) {
    GURL gurl(token_url_);
    CHECK(gurl.is_valid());
    chromeos::Uri uri(endpoint_uri);
    CHECK(uri.GetLastParsingError().status ==
          chromeos::Uri::ParserStatus::kNoErrors);
    session_ = std::make_unique<IppEndpointTokenFetcher>(
        server_.GetURLLoaderFactory(), gurl, uri, std::move(scope));
  }

  // URL of the endpoint at the server.
  const std::string token_url_ = "https://a.b/c";
  // The object simulating the Authorization Server.
  FakeAuthorizationServer server_;
  // The tested session, it is created by the method CreateSession(...).
  std::unique_ptr<IppEndpointTokenFetcher> session_;
};

TEST_F(PrintingOAuth2IppEndpointTokenFetcherTest, InitialState) {
  CreateSession("ipp://my.printer/print", {"ala", "ma", "kota"});
  EXPECT_EQ(session_->ipp_endpoint_uri().GetNormalized(false),
            "ipp://my.printer/print");
  EXPECT_EQ(session_->scope(),
            base::flat_set<std::string>({"ala", "ma", "kota"}));
  EXPECT_TRUE(session_->endpoint_access_token().empty());
  EXPECT_TRUE(session_->TakeWaitingList().empty());
}

TEST_F(PrintingOAuth2IppEndpointTokenFetcherTest, WaitingList) {
  CreateSession("ipp://my.printer/print", {"ala", "ma", "kota"});
  CallbackResult cr1;
  CallbackResult cr2;
  CallbackResult cr3;
  session_->AddToWaitingList(BindResult(cr1));
  session_->AddToWaitingList(BindResult(cr2));
  session_->AddToWaitingList(BindResult(cr3));
  auto callbacks = session_->TakeWaitingList();
  ASSERT_EQ(callbacks.size(), 3u);
  EXPECT_TRUE(session_->TakeWaitingList().empty());
  std::move(callbacks[0]).Run(StatusCode::kOK, "1");
  std::move(callbacks[1]).Run(StatusCode::kAccessDenied, "2");
  std::move(callbacks[2]).Run(StatusCode::kServerError, "3");
  EXPECT_EQ(cr1.status, StatusCode::kOK);
  EXPECT_EQ(cr1.data, "1");
  EXPECT_EQ(cr2.status, StatusCode::kAccessDenied);
  EXPECT_EQ(cr2.data, "2");
  EXPECT_EQ(cr3.status, StatusCode::kServerError);
  EXPECT_EQ(cr3.data, "3");
}

TEST_F(PrintingOAuth2IppEndpointTokenFetcherTest, TokenExchangeRequest) {
  CreateSession("ipp://my.printer:123/print", {});
  CallbackResult cr;
  session_->SendTokenExchangeRequest("access_token_s%!", BindResult(cr));

  // Verify the request.
  base::flat_map<std::string, std::string> params;
  ASSERT_EQ("", server_.ReceivePOSTWithURLParams(token_url_, params));
  EXPECT_EQ(params["grant_type"],
            "urn:ietf:params:oauth:grant-type:token-exchange");
  EXPECT_EQ(params["resource"], "ipp://my.printer:123/print");
  EXPECT_EQ(params["subject_token"], "access_token_s%!");
  EXPECT_EQ(params["subject_token_type"],
            "urn:ietf:params:oauth:token-type:access_token");

  // Prepare and send the response.
  base::Value::Dict fields;
  fields.Set("access_token", "endpoint_access_token_swD");
  fields.Set("issued_token_type",
             "urn:ietf:params:oauth:token-type:access_token");
  fields.Set("token_type", "bearer");
  server_.ResponseWithJSON(net::HttpStatusCode::HTTP_OK, fields);

  // Verify the response.
  EXPECT_EQ(cr.status, StatusCode::kOK);
  EXPECT_EQ(cr.data, "endpoint_access_token_swD");
  EXPECT_EQ(session_->endpoint_access_token(), "endpoint_access_token_swD");
}

TEST_F(PrintingOAuth2IppEndpointTokenFetcherTest, TokenExchangeRequestError) {
  CreateSession("ipp://my.printer:123/print", {});
  CallbackResult cr;
  session_->SendTokenExchangeRequest("access_token_s%!", BindResult(cr));

  // Receive the request and send the response.
  base::flat_map<std::string, std::string> params;
  ASSERT_EQ("", server_.ReceivePOSTWithURLParams(token_url_, params));
  base::Value::Dict fields;
  fields.Set("access_token", "endpoint_access_token_swD");
  fields.Set("issued_token_type",
             "urn:ietf:params:oauth:token-type:access_token");
  // Missing field "token_type".
  server_.ResponseWithJSON(net::HttpStatusCode::HTTP_OK, fields);

  // Verify the response.
  EXPECT_EQ(cr.status, StatusCode::kInvalidResponse);
  // The error message contains "token_type".
  EXPECT_NE(cr.data.find("token_type"), std::string::npos);
  EXPECT_TRUE(session_->endpoint_access_token().empty());
}

TEST_F(PrintingOAuth2IppEndpointTokenFetcherTest, InvalidAccessToken) {
  CreateSession("ipp://my.printer:123/print", {});
  CallbackResult cr;
  session_->SendTokenExchangeRequest("access_token_s%!", BindResult(cr));

  // Receive the request and send the response.
  base::flat_map<std::string, std::string> params;
  ASSERT_EQ("", server_.ReceivePOSTWithURLParams(token_url_, params));
  base::Value::Dict fields;
  fields.Set("error", "invalid_grant");
  server_.ResponseWithJSON(net::HttpStatusCode::HTTP_BAD_REQUEST, fields);

  // Verify the response.
  EXPECT_EQ(cr.status, StatusCode::kInvalidAccessToken);
  EXPECT_EQ(cr.data, "access_token_s%!");
  EXPECT_TRUE(session_->endpoint_access_token().empty());
}

}  // namespace
}  // namespace oauth2
}  // namespace printing
}  // namespace ash
