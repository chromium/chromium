// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/oauth2/authorization_server_session.h"

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

class PrintingOAuth2AuthorizationServerSessionTest : public testing::Test {
 protected:
  // Initialize the `session_` field.
  void CreateSession(base::flat_set<std::string>&& scope) {
    GURL gurl(url_);
    CHECK(gurl.is_valid());
    session_ = std::make_unique<AuthorizationServerSession>(
        server_.GetURLLoaderFactory(), gurl, std::move(scope));
  }
  // Process the First Token request on the server side and send a response
  // with given `access_token` and `refresh_token`. If the `refresh_token` is
  // empty the response does not contain the field "refresh_token".
  void ProcessFirstTokenRequestAndResponse(
      const std::string& access_token,
      const std::string& refresh_token = "") {
    base::flat_map<std::string, std::string> params;
    base::Value::Dict fields;
    EXPECT_EQ("", server_.ReceivePOSTWithURLParams(url_, params));
    fields.Set("access_token", access_token);
    fields.Set("token_type", "bearer");
    if (!refresh_token.empty()) {
      fields.Set("refresh_token", refresh_token);
    }
    server_.ResponseWithJSON(net::HttpStatusCode::HTTP_OK, fields);
  }
  // URL of the endpoint at the server.
  const std::string url_ = "https://a.b/c";
  // The object simulating the Authorization Server.
  FakeAuthorizationServer server_;
  // The testes session, it is created by the method CreateSession(...).
  std::unique_ptr<AuthorizationServerSession> session_;
};

TEST_F(PrintingOAuth2AuthorizationServerSessionTest, ParseScope) {
  auto scope = ParseScope("  w szczebrzeszynie   chrzaszcz brzmi w trzcinie ");
  EXPECT_EQ(scope.size(), 5u);
  EXPECT_TRUE(scope.contains("w"));
  EXPECT_TRUE(scope.contains("szczebrzeszynie"));
  EXPECT_TRUE(scope.contains("chrzaszcz"));
  EXPECT_TRUE(scope.contains("brzmi"));
  EXPECT_TRUE(scope.contains("trzcinie"));
}

TEST_F(PrintingOAuth2AuthorizationServerSessionTest, InitialState) {
  CreateSession({"ala", "ma", "kota"});
  EXPECT_TRUE(session_->access_token().empty());
  EXPECT_TRUE(session_->ContainsAll({}));
  EXPECT_TRUE(session_->ContainsAll({"kota", "ala"}));
  EXPECT_FALSE(session_->ContainsAll({"psa", "ma"}));
}

TEST_F(PrintingOAuth2AuthorizationServerSessionTest, WaitingList) {
  CreateSession({});
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

TEST_F(PrintingOAuth2AuthorizationServerSessionTest, FirstTokenRequest) {
  CreateSession({"xxx"});
  CallbackResult cr;
  session_->SendFirstTokenRequest("clientID_xe2$", "auth_code_3d#x",
                                  "code_verifier_P2s&", BindResult(cr));

  // Verify the request.
  base::flat_map<std::string, std::string> params;
  ASSERT_EQ("", server_.ReceivePOSTWithURLParams(url_, params));
  EXPECT_EQ(params["grant_type"], "authorization_code");
  EXPECT_EQ(params["code"], "auth_code_3d#x");
  EXPECT_EQ(params["redirect_uri"], printing::oauth2::kRedirectURI);
  EXPECT_EQ(params["client_id"], "clientID_xe2$");
  EXPECT_EQ(params["code_verifier"], "code_verifier_P2s&");

  // Prepare and send the response.
  base::Value::Dict fields;
  fields.Set("access_token", "access_token_@(#a");
  fields.Set("token_type", "bearer");
  fields.Set("refresh_token", "refresh_token_X)(@K");
  server_.ResponseWithJSON(net::HttpStatusCode::HTTP_OK, fields);

  // Verify the response.
  EXPECT_EQ(cr.status, StatusCode::kOK);
  EXPECT_EQ(cr.data, "access_token_@(#a");
  EXPECT_EQ(session_->access_token(), "access_token_@(#a");
}

TEST_F(PrintingOAuth2AuthorizationServerSessionTest, FirstTokenRequestError) {
  CreateSession({"xxx"});
  CallbackResult cr;
  session_->SendFirstTokenRequest("a", "b", "c", BindResult(cr));

  // Receive the request.
  base::flat_map<std::string, std::string> params;
  ASSERT_EQ("", server_.ReceivePOSTWithURLParams(url_, params));
  EXPECT_EQ(params["code"], "b");
  EXPECT_EQ(params["client_id"], "a");
  EXPECT_EQ(params["code_verifier"], "c");

  // Prepare and send the response.
  base::Value::Dict fields;
  fields.Set("access_token", "access_token_1");
  // The field "token_type" is wrong.
  fields.Set("token_type", "bearer_WRONG");
  fields.Set("refresh_token", "refresh_token_2");
  server_.ResponseWithJSON(net::HttpStatusCode::HTTP_OK, fields);

  // Verify the response.
  EXPECT_EQ(cr.status, StatusCode::kInvalidResponse);
  // The error message contains "token_type".
  EXPECT_NE(cr.data.find("token_type"), std::string::npos);
  EXPECT_TRUE(session_->access_token().empty());
}

TEST_F(PrintingOAuth2AuthorizationServerSessionTest, NextTokenRequest) {
  CreateSession({"xxx"});
  CallbackResult cr;

  // Send the First Token Request and process it.
  session_->SendFirstTokenRequest("a", "b", "c", BindResult(cr));
  ProcessFirstTokenRequestAndResponse("access_token_1", "refresh_token_X)(@K");

  // Receive the response and send the Next Token Request.
  ASSERT_EQ(cr.status, StatusCode::kOK);
  EXPECT_EQ(session_->access_token(), "access_token_1");
  session_->SendNextTokenRequest(BindResult(cr));
  // It should reset the current access token.
  EXPECT_TRUE(session_->access_token().empty());

  // Receive and verify the request.
  base::flat_map<std::string, std::string> params;
  ASSERT_EQ("", server_.ReceivePOSTWithURLParams(url_, params));
  EXPECT_EQ(params["grant_type"], "refresh_token");
  EXPECT_EQ(params["refresh_token"], "refresh_token_X)(@K");

  // Prepare and send the response.
  base::Value::Dict fields;
  fields.Set("access_token", "new_access_token_123");
  fields.Set("token_type", "bearer");
  server_.ResponseWithJSON(net::HttpStatusCode::HTTP_OK, fields);

  // Verify the response.
  EXPECT_EQ(cr.status, StatusCode::kOK);
  EXPECT_EQ(cr.data, "new_access_token_123");
  EXPECT_EQ(session_->access_token(), "new_access_token_123");
}

TEST_F(PrintingOAuth2AuthorizationServerSessionTest, NextTokenRequestError) {
  CreateSession({"xxx"});
  CallbackResult cr;

  // Send the First Token Request and process it.
  session_->SendFirstTokenRequest("a", "b", "c", BindResult(cr));
  ProcessFirstTokenRequestAndResponse("access_token_1", "refresh_token_X)(@K");

  // Receive the response and send the Next Token Request.
  ASSERT_EQ(cr.status, StatusCode::kOK);
  EXPECT_EQ(session_->access_token(), "access_token_1");
  session_->SendNextTokenRequest(BindResult(cr));

  // Receive the request and send the response.
  base::flat_map<std::string, std::string> params;
  ASSERT_EQ("", server_.ReceivePOSTWithURLParams(url_, params));
  EXPECT_EQ(params["refresh_token"], "refresh_token_X)(@K");
  base::Value::Dict fields;
  fields.Set("token_type", "bearer");
  // The field "access_token" is missing.
  server_.ResponseWithJSON(net::HttpStatusCode::HTTP_OK, fields);

  // Verify the response.
  EXPECT_EQ(cr.status, StatusCode::kInvalidResponse);
  // The error message contains "access_token".
  EXPECT_NE(cr.data.find("access_token"), std::string::npos);
  EXPECT_TRUE(session_->access_token().empty());
}

TEST_F(PrintingOAuth2AuthorizationServerSessionTest, NoRefreshTokens) {
  CreateSession({});
  CallbackResult cr;

  // Send the First Token Request and process it.
  session_->SendFirstTokenRequest("a", "b", "c", BindResult(cr));
  ProcessFirstTokenRequestAndResponse("access_token_@(#a");

  // Receive the response and send the Next Token Request.
  ASSERT_EQ(cr.status, StatusCode::kOK);
  EXPECT_EQ(cr.data, "access_token_@(#a");
  session_->SendNextTokenRequest(BindResult(cr));

  // Verify the response.
  EXPECT_EQ(cr.status, StatusCode::kAuthorizationNeeded);
  EXPECT_TRUE(session_->access_token().empty());
}

TEST_F(PrintingOAuth2AuthorizationServerSessionTest, InvalidRefreshToken) {
  CreateSession({"xxx"});
  CallbackResult cr;

  // Send the First Token Request and process it.
  session_->SendFirstTokenRequest("a", "b", "c", BindResult(cr));
  ProcessFirstTokenRequestAndResponse("access_token_1", "refresh_token_X)@K");

  // Receive the response and send the Next Token Request.
  ASSERT_EQ(cr.status, StatusCode::kOK);
  session_->SendNextTokenRequest(BindResult(cr));

  // Receive and the request and send the response.
  base::flat_map<std::string, std::string> params;
  ASSERT_EQ("", server_.ReceivePOSTWithURLParams(url_, params));
  EXPECT_EQ(params["refresh_token"], "refresh_token_X)@K");
  base::Value::Dict fields;
  fields.Set("error", "invalid_grant");
  server_.ResponseWithJSON(net::HttpStatusCode::HTTP_BAD_REQUEST, fields);

  // Verify the response.
  EXPECT_EQ(cr.status, StatusCode::kAuthorizationNeeded);
  EXPECT_TRUE(session_->access_token().empty());
}

}  // namespace
}  // namespace oauth2
}  // namespace printing
}  // namespace ash
