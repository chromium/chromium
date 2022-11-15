// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/oauth2/authorization_zone.h"

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chrome/browser/ash/printing/oauth2/constants.h"
#include "chrome/browser/ash/printing/oauth2/mock_client_ids_database.h"
#include "chrome/browser/ash/printing/oauth2/test_authorization_server.h"
#include "chromeos/printing/uri.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash {
namespace printing {
namespace oauth2 {
namespace {

class PrintingOAuth2AuthorizationZoneTest : public testing::Test {
 public:
  // Initializes Authorization Zone.
  void CreateAuthorizationZone(const std::string& client_id) {
    GURL auth_server_uri(authorization_server_uri_);
    CHECK(auth_server_uri.is_valid());

    EXPECT_CALL(client_ids_database_, FetchId)
        .WillOnce([client_id](const GURL& url, StatusCallback callback) {
          std::move(callback).Run(StatusCode::kOK, client_id);
        });
    authorization_zone_ = printing::oauth2::AuthorizationZone::Create(
        server_.GetURLLoaderFactory(), auth_server_uri, &client_ids_database_);
  }

  // Simulates the authorization process in the internet browser. Returns the
  // URL that the browser is redirected to at the end of the authorization
  // process.
  std::string SimulateAuthorization(const std::string& authorization_url,
                                    const std::string& auth_code,
                                    const std::string& scope) {
    auto question_mark = authorization_url.find('?');
    EXPECT_LT(question_mark, authorization_url.size());
    const std::string auth_host_path =
        authorization_url.substr(0, question_mark);
    base::flat_map<std::string, std::string> params;
    EXPECT_TRUE(ParseURLParameters(authorization_url.substr(question_mark + 1),
                                   params));
    EXPECT_EQ(params["scope"], scope);
    return base::StrCat(
        {printing::oauth2::kRedirectURI,
         "?code=", base::EscapeUrlEncodedData(auth_code, true),
         "&state=", base::EscapeUrlEncodedData(params["state"], true)});
  }

  std::string SimulateAuthorizationError(const std::string& authorization_url,
                                         const std::string& error) {
    auto question_mark = authorization_url.find('?');
    EXPECT_LT(question_mark, authorization_url.size());
    const std::string auth_host_path =
        authorization_url.substr(0, question_mark);
    base::flat_map<std::string, std::string> params;
    EXPECT_TRUE(ParseURLParameters(authorization_url.substr(question_mark + 1),
                                   params));
    return base::StrCat(
        {printing::oauth2::kRedirectURI,
         "?error=", base::EscapeUrlEncodedData(error, true),
         "&state=", base::EscapeUrlEncodedData(params["state"], true)});
  }

  // Simulates Metadata Request described in rfc8414, section 3.
  void ProcessMetadataRequest() {
    EXPECT_EQ("", server_.ReceiveGET(metadata_uri_));
    auto fields = BuildMetadata(authorization_server_uri_, authorization_uri_,
                                token_uri_, registration_uri_);
    server_.ResponseWithJSON(net::HttpStatusCode::HTTP_OK, fields);
  }

  // Simulates Registration Request described in rfc7591, section 3.
  void ProcessRegistrationRequest(const std::string& client_id) {
    base::Value::Dict fields;
    EXPECT_EQ("", server_.ReceivePOSTWithJSON(registration_uri_, fields));
    fields.Set("client_id", client_id);
    server_.ResponseWithJSON(net::HttpStatusCode::HTTP_CREATED, fields);
  }

  // Simulates First Token Request described in rfc6749, sections 4.1.3-4 and 5.
  void ProcessFirstTokenRequest(const std::string& auth_code,
                                const std::string& access_token,
                                const std::string& refresh_token) {
    base::flat_map<std::string, std::string> params;
    EXPECT_EQ("", server_.ReceivePOSTWithURLParams(token_uri_, params));
    EXPECT_EQ(params["code"], auth_code);
    base::Value::Dict fields;
    fields.Set("access_token", access_token);
    fields.Set("token_type", "bearer");
    if (!refresh_token.empty()) {
      fields.Set("refresh_token", refresh_token);
    }
    server_.ResponseWithJSON(net::HttpStatusCode::HTTP_OK, fields);
  }

  // The same as ProcessFirstTokenRequest(...) but with an error response.
  void ProcessFirstTokenRequestError(const std::string& auth_code,
                                     const std::string& error) {
    base::flat_map<std::string, std::string> params;
    EXPECT_EQ("", server_.ReceivePOSTWithURLParams(token_uri_, params));
    EXPECT_EQ(params["code"], auth_code);
    base::Value::Dict fields;
    fields.Set("error", error);
    server_.ResponseWithJSON(net::HttpStatusCode::HTTP_BAD_REQUEST, fields);
  }

  // Simulates Next Token Request described in rfc6749, section 6.
  void ProcessNextTokenRequest(const std::string& current_refresh_token,
                               const std::string& access_token,
                               const std::string& new_refresh_token) {
    base::flat_map<std::string, std::string> params;
    EXPECT_EQ("", server_.ReceivePOSTWithURLParams(token_uri_, params));
    EXPECT_EQ(params["refresh_token"], current_refresh_token);
    base::Value::Dict fields;
    fields.Set("access_token", access_token);
    fields.Set("token_type", "bearer");
    if (!new_refresh_token.empty()) {
      fields.Set("refresh_token", new_refresh_token);
    }
    server_.ResponseWithJSON(net::HttpStatusCode::HTTP_OK, fields);
  }

  // Simulates Token Exchange Request described in rfc8693, section 2.
  void ProcessTokenExchangeRequest(const chromeos::Uri& ipp_endpoint,
                                   const std::string& access_token,
                                   const std::string& endpoint_access_token) {
    base::flat_map<std::string, std::string> params;
    EXPECT_EQ("", server_.ReceivePOSTWithURLParams(token_uri_, params));
    EXPECT_EQ(params["resource"], ipp_endpoint.GetNormalized());
    EXPECT_EQ(params["subject_token"], access_token);
    base::Value::Dict fields;
    fields.Set("access_token", endpoint_access_token);
    fields.Set("issued_token_type",
               "urn:ietf:params:oauth:token-type:access_token");
    fields.Set("token_type", "bearer");
    server_.ResponseWithJSON(net::HttpStatusCode::HTTP_OK, fields);
  }

  // The same as ProcessTokenExchangeRequest(...) but with an error response.
  void ProcessTokenExchangeRequestError(const chromeos::Uri& ipp_endpoint,
                                        const std::string& access_token,
                                        const std::string& error) {
    base::flat_map<std::string, std::string> params;
    EXPECT_EQ("", server_.ReceivePOSTWithURLParams(token_uri_, params));
    EXPECT_EQ(params["resource"], ipp_endpoint.GetNormalized());
    EXPECT_EQ(params["subject_token"], access_token);
    base::Value::Dict fields;
    fields.Set("error", error);
    server_.ResponseWithJSON(net::HttpStatusCode::HTTP_BAD_REQUEST, fields);
  }

 protected:
  const std::string authorization_server_uri_ = "https://example.com/path";
  const std::string metadata_uri_ =
      "https://example.com/.well-known/oauth-authorization-server/path";
  const std::string authorization_uri_ = "https://example.com/authorization";
  const std::string token_uri_ = "https://example.com/token";
  const std::string registration_uri_ = "https://example.com/registration";

  testing::NiceMock<MockClientIdsDatabase> client_ids_database_;
  std::unique_ptr<printing::oauth2::AuthorizationZone> authorization_zone_;
  FakeAuthorizationServer server_;
};

TEST_F(PrintingOAuth2AuthorizationZoneTest, InitializationOfRegisteredClient) {
  CallbackResult cr;
  CreateAuthorizationZone("clientID_abcd1234");

  authorization_zone_->InitAuthorization("scope1 scope2 scope3",
                                         BindResult(cr));
  ProcessMetadataRequest();
  ASSERT_EQ(cr.status, printing::oauth2::StatusCode::kOK);
  const std::string authorization_url = cr.data;

  // Parse and verify the returned URL.
  auto question_mark = authorization_url.find('?');
  ASSERT_LT(question_mark, authorization_url.size());
  const std::string auth_host_path = authorization_url.substr(0, question_mark);
  base::flat_map<std::string, std::string> params;
  ASSERT_TRUE(
      ParseURLParameters(authorization_url.substr(question_mark + 1), params));
  EXPECT_EQ(auth_host_path, authorization_uri_);
  EXPECT_EQ(params["response_type"], "code");
  EXPECT_EQ(params["response_mode"], "query");
  EXPECT_EQ(params["client_id"], "clientID_abcd1234");
  EXPECT_EQ(params["redirect_uri"], printing::oauth2::kRedirectURI);
  EXPECT_EQ(params["scope"], "scope1 scope2 scope3");
  EXPECT_FALSE(params["code_challenge"].empty());
  EXPECT_EQ(params["code_challenge_method"], "S256");
  EXPECT_FALSE(params["state"].empty());
}

TEST_F(PrintingOAuth2AuthorizationZoneTest,
       InitializationOfUnregisteredClient) {
  CallbackResult cr;
  CreateAuthorizationZone("");

  authorization_zone_->InitAuthorization("", BindResult(cr));
  ProcessMetadataRequest();
  ProcessRegistrationRequest("clientID_!@#$");
  EXPECT_EQ(cr.status, printing::oauth2::StatusCode::kOK);
  const std::string authorization_url = cr.data;

  // Parse and verify the returned URL.
  auto question_mark = authorization_url.find('?');
  ASSERT_LT(question_mark, authorization_url.size());
  const std::string auth_host_path = authorization_url.substr(0, question_mark);
  base::flat_map<std::string, std::string> params;
  ASSERT_TRUE(
      ParseURLParameters(authorization_url.substr(question_mark + 1), params));
  EXPECT_EQ(params["client_id"], "clientID_!@#$");
  EXPECT_EQ(params.count("scope"), 0u);
}

TEST_F(PrintingOAuth2AuthorizationZoneTest, FirstAccessToken) {
  CallbackResult cr;
  chromeos::Uri ipp_endpoint("ipp://my.printer:1234/path");
  CreateAuthorizationZone("clientID_!@#$");
  authorization_zone_->InitAuthorization("", BindResult(cr));
  ProcessMetadataRequest();
  ASSERT_EQ(cr.status, printing::oauth2::StatusCode::kOK);
  auto resultant_url = SimulateAuthorization(cr.data, "auth_code_123", "");
  authorization_zone_->FinishAuthorization(GURL(resultant_url), BindResult(cr));
  ProcessFirstTokenRequest("auth_code_123", "access_TOKEN", "refresh_TOKEN");
  ASSERT_EQ(cr.status, printing::oauth2::StatusCode::kOK);
  EXPECT_TRUE(cr.data.empty());
}

TEST_F(PrintingOAuth2AuthorizationZoneTest, AuthorizationFail) {
  CallbackResult cr;
  chromeos::Uri ipp_endpoint("ipp://my.printer:1234/path");
  CreateAuthorizationZone("clientID_!@#$");
  authorization_zone_->InitAuthorization("", BindResult(cr));
  ProcessMetadataRequest();
  ASSERT_EQ(cr.status, printing::oauth2::StatusCode::kOK);
  auto resultant_url = SimulateAuthorizationError(cr.data, "weird_problem");
  authorization_zone_->FinishAuthorization(GURL(resultant_url), BindResult(cr));
  ASSERT_EQ(cr.status, printing::oauth2::StatusCode::kAccessDenied);
  // The error message contains "weird_problem".
  EXPECT_NE(cr.data.find("weird_problem"), std::string::npos);
}

TEST_F(PrintingOAuth2AuthorizationZoneTest,
       AuthorizationInvalidResponseEmptyCode) {
  CallbackResult cr;
  chromeos::Uri ipp_endpoint("ipp://my.printer:1234/path");
  CreateAuthorizationZone("clientID_!@#$");
  authorization_zone_->InitAuthorization("", BindResult(cr));
  ProcessMetadataRequest();
  ASSERT_EQ(cr.status, printing::oauth2::StatusCode::kOK);
  // The URL the browser was redirected to has empty "code" param.
  auto resultant_url = SimulateAuthorization(cr.data, "", "");
  authorization_zone_->FinishAuthorization(GURL(resultant_url), BindResult(cr));
  EXPECT_EQ(cr.status, printing::oauth2::StatusCode::kInvalidResponse);
  // The error message contains "code".
  EXPECT_NE(cr.data.find("code"), std::string::npos);
}

TEST_F(PrintingOAuth2AuthorizationZoneTest,
       AuthorizationInvalidResponseNoState) {
  CallbackResult cr;
  chromeos::Uri ipp_endpoint("ipp://my.printer:1234/path");
  CreateAuthorizationZone("clientID_!@#$");
  authorization_zone_->InitAuthorization("", BindResult(cr));
  ProcessMetadataRequest();
  ASSERT_EQ(cr.status, printing::oauth2::StatusCode::kOK);
  // The URL the browser was redirected to has missing "state" param.
  std::string resultant_url = printing::oauth2::kRedirectURI;
  resultant_url += "?code=authCode123";
  authorization_zone_->FinishAuthorization(GURL(resultant_url), BindResult(cr));
  EXPECT_EQ(cr.status, printing::oauth2::StatusCode::kInvalidResponse);
  // The error message contains "state".
  EXPECT_NE(cr.data.find("state"), std::string::npos);
}

TEST_F(PrintingOAuth2AuthorizationZoneTest, FirstAccessTokenFail) {
  CallbackResult cr;
  chromeos::Uri ipp_endpoint("ipp://my.printer:1234/path");
  CreateAuthorizationZone("clientID_!@#$");
  authorization_zone_->InitAuthorization("", BindResult(cr));
  ProcessMetadataRequest();
  ASSERT_EQ(cr.status, printing::oauth2::StatusCode::kOK);
  auto resultant_url = SimulateAuthorization(cr.data, "auth_code_123", "");
  authorization_zone_->FinishAuthorization(GURL(resultant_url), BindResult(cr));
  ProcessFirstTokenRequestError("auth_code_123", "my unknown error");
  EXPECT_EQ(cr.status, printing::oauth2::StatusCode::kAccessDenied);
  // The error message contains "my unknown error".
  EXPECT_NE(cr.data.find("my unknown error"), std::string::npos);
}

TEST_F(PrintingOAuth2AuthorizationZoneTest, TokenRefresh) {
  CallbackResult cr;
  chromeos::Uri ipp_endpoint("ipp://my.printer:1234/path");
  CreateAuthorizationZone("clientID_!@#$");
  authorization_zone_->InitAuthorization("", BindResult(cr));
  ProcessMetadataRequest();
  ASSERT_EQ(cr.status, printing::oauth2::StatusCode::kOK);
  auto resultant_url = SimulateAuthorization(cr.data, "auth_code_123", "");
  authorization_zone_->FinishAuthorization(GURL(resultant_url), BindResult(cr));
  ProcessFirstTokenRequest("auth_code_123", "access_TOKEN", "refresh_TOKEN");
  ASSERT_EQ(cr.status, printing::oauth2::StatusCode::kOK);
  authorization_zone_->GetEndpointAccessToken(ipp_endpoint, "", BindResult(cr));
  ProcessTokenExchangeRequest(ipp_endpoint, "access_TOKEN",
                              "endpoint_token_24$#D");
  ASSERT_EQ(cr.status, printing::oauth2::StatusCode::kOK);
  authorization_zone_->MarkEndpointAccessTokenAsExpired(ipp_endpoint,
                                                        "endpoint_token_24$#D");
  authorization_zone_->GetEndpointAccessToken(ipp_endpoint, "", BindResult(cr));
  ProcessTokenExchangeRequestError(ipp_endpoint, "access_TOKEN",
                                   "invalid_grant");
  ProcessNextTokenRequest("refresh_TOKEN", "access_token2_w5%",
                          "refresh_token2_1dh");
  ProcessTokenExchangeRequest(ipp_endpoint, "access_token2_w5%",
                              "endpoint_token2_E46h");
  EXPECT_EQ(cr.status, printing::oauth2::StatusCode::kOK);
  EXPECT_EQ(cr.data, "endpoint_token2_E46h");
}

TEST_F(PrintingOAuth2AuthorizationZoneTest, ParallelRequestsWithScopes) {
  CallbackResult cr_ia1;
  CallbackResult cr_ia2;
  CallbackResult cr_ia3;
  chromeos::Uri ipp_endpoint_1("ipp://my.printer:1234/path");
  chromeos::Uri ipp_endpoint_2("ipp://my.other_printer:123/path");
  CreateAuthorizationZone("clientID");
  authorization_zone_->InitAuthorization("scope0", BindResult(cr_ia1));
  authorization_zone_->InitAuthorization("scope1 scope2", BindResult(cr_ia2));
  authorization_zone_->InitAuthorization("scope2", BindResult(cr_ia3));
  ProcessMetadataRequest();
  ASSERT_EQ(cr_ia1.status, printing::oauth2::StatusCode::kOK);
  ASSERT_EQ(cr_ia2.status, printing::oauth2::StatusCode::kOK);
  ASSERT_EQ(cr_ia3.status, printing::oauth2::StatusCode::kOK);
  auto auth_url_1 = SimulateAuthorization(cr_ia1.data, "auth_code_1", "scope0");
  auto auth_url_2 =
      SimulateAuthorization(cr_ia2.data, "auth_code_2", "scope1 scope2");
  auto auth_url_3 = SimulateAuthorization(cr_ia3.data, "auth_code_3", "scope2");
  authorization_zone_->FinishAuthorization(GURL(auth_url_3),
                                           BindResult(cr_ia3));
  authorization_zone_->FinishAuthorization(GURL(auth_url_1),
                                           BindResult(cr_ia1));
  authorization_zone_->FinishAuthorization(GURL(auth_url_2),
                                           BindResult(cr_ia2));
  ProcessFirstTokenRequest("auth_code_3", "acc_token_3", "ref_token_3");
  ProcessFirstTokenRequest("auth_code_1", "acc_token_1", "ref_token_1");
  ProcessFirstTokenRequest("auth_code_2", "acc_token_2", "ref_token_2");
  ASSERT_EQ(cr_ia1.status, printing::oauth2::StatusCode::kOK);
  ASSERT_EQ(cr_ia2.status, printing::oauth2::StatusCode::kOK);
  ASSERT_EQ(cr_ia3.status, printing::oauth2::StatusCode::kOK);
  authorization_zone_->GetEndpointAccessToken(ipp_endpoint_1, "scope0",
                                              BindResult(cr_ia1));
  authorization_zone_->GetEndpointAccessToken(ipp_endpoint_2, "scope1",
                                              BindResult(cr_ia2));
  authorization_zone_->GetEndpointAccessToken(ipp_endpoint_1, "scope2 scope1",
                                              BindResult(cr_ia3));
  ProcessTokenExchangeRequest(ipp_endpoint_1, "acc_token_1", "end_token_1");
  ProcessTokenExchangeRequest(ipp_endpoint_2, "acc_token_2", "end_token_2");
  // The third GetEndpointAccessToken(...) call used the first token as the
  // first one.
  EXPECT_EQ(cr_ia1.status, printing::oauth2::StatusCode::kOK);
  EXPECT_EQ(cr_ia2.status, printing::oauth2::StatusCode::kOK);
  EXPECT_EQ(cr_ia3.status, printing::oauth2::StatusCode::kOK);
  EXPECT_EQ(cr_ia1.data, "end_token_1");
  EXPECT_EQ(cr_ia2.data, "end_token_2");
  EXPECT_EQ(cr_ia3.data, "end_token_1");
}

TEST_F(PrintingOAuth2AuthorizationZoneTest, ParallelInitializations) {
  CreateAuthorizationZone("clientID");
  std::vector<CallbackResult> crs(kMaxNumberOfSessions + 1);

  // Too many initializations. The oldest one is going to be rejected.
  for (auto& cr : crs) {
    authorization_zone_->InitAuthorization("scope", BindResult(cr));
  }
  EXPECT_EQ(crs[0].status, printing::oauth2::StatusCode::kTooManySessions);
  ProcessMetadataRequest();
  for (size_t i = 1; i < crs.size(); ++i) {
    EXPECT_EQ(crs[i].status, printing::oauth2::StatusCode::kOK);
  }
}

TEST_F(PrintingOAuth2AuthorizationZoneTest, ParallelAuthorizations) {
  CreateAuthorizationZone("clientID");
  std::vector<CallbackResult> crs(kMaxNumberOfSessions + 1);

  // Call the first InitAuthorization(...) to go through initialization.
  authorization_zone_->InitAuthorization("", BindResult(crs[0]));
  ProcessMetadataRequest();
  ASSERT_EQ(crs[0].status, printing::oauth2::StatusCode::kOK);
  // Call InitAuthorization(...) kMaxNumberOfSessions times.
  for (size_t i = 1; i < crs.size(); ++i) {
    authorization_zone_->InitAuthorization("", BindResult(crs[i]));
    ASSERT_EQ(crs[i].status, printing::oauth2::StatusCode::kOK);
  }

  // Call all corresponding FinishAuthorization(...).
  for (size_t i = 0; i < crs.size(); ++i) {
    auto auth_url = SimulateAuthorization(
        crs[i].data, "auth_code_" + base::NumberToString(i), "");
    authorization_zone_->FinishAuthorization(GURL(auth_url),
                                             BindResult(crs[i]));
  }

  // The first call to FinishAuthorization(...) failed because the pending
  // authorization is missing (was removed as the oldest one). Other calls
  // succeeded.
  EXPECT_EQ(crs[0].status, printing::oauth2::StatusCode::kNoMatchingSession);
  for (size_t i = 1; i < crs.size(); ++i) {
    const std::string si = base::NumberToString(i);
    ProcessFirstTokenRequest("auth_code_" + si, "acc_token_" + si,
                             "ref_token_" + si);
    EXPECT_EQ(crs[i].status, printing::oauth2::StatusCode::kOK);
  }
}

TEST_F(PrintingOAuth2AuthorizationZoneTest, ParallelFirstTokenRequests) {
  CreateAuthorizationZone("clientID");
  std::vector<CallbackResult> crs(kMaxNumberOfSessions + 1);

  // Complete the first authorization and get an endpoint access token.
  authorization_zone_->InitAuthorization("scope0", BindResult(crs[0]));
  ProcessMetadataRequest();
  ASSERT_EQ(crs[0].status, printing::oauth2::StatusCode::kOK);
  auto auth_url_0 = SimulateAuthorization(crs[0].data, "auth_code_0", "scope0");
  authorization_zone_->FinishAuthorization(GURL(auth_url_0),
                                           BindResult(crs[0]));
  ProcessFirstTokenRequest("auth_code_0", "acc_token_0", "ref_token_0");
  ASSERT_EQ(crs[0].status, printing::oauth2::StatusCode::kOK);
  authorization_zone_->GetEndpointAccessToken(chromeos::Uri("ipp://whatever:1"),
                                              "scope0", BindResult(crs[0]));
  ProcessTokenExchangeRequest(chromeos::Uri("ipp://whatever:1"), "acc_token_0",
                              "xxx");
  EXPECT_EQ(crs[0].status, printing::oauth2::StatusCode::kOK);

  // Start kMaxNumberOfSessions other sessions (with different scope).
  for (size_t i = 1; i < crs.size(); ++i) {
    const std::string si = base::NumberToString(i);
    authorization_zone_->InitAuthorization("", BindResult(crs[i]));
    ASSERT_EQ(crs[i].status, printing::oauth2::StatusCode::kOK);
    auto auth_url = SimulateAuthorization(crs[i].data, "auth_code_" + si, "");
    authorization_zone_->FinishAuthorization(GURL(auth_url),
                                             BindResult(crs[i]));
    ProcessFirstTokenRequest("auth_code_" + si, "acc_token_" + si,
                             "ref_token_" + si);
    EXPECT_EQ(crs[i].status, printing::oauth2::StatusCode::kOK);
  }

  // The first session was closed because it was the oldest one and max allowed
  // number of sessions was reached.
  authorization_zone_->GetEndpointAccessToken(chromeos::Uri("ipp://whatever:2"),
                                              "scope0", BindResult(crs[0]));
  EXPECT_EQ(crs[0].status, printing::oauth2::StatusCode::kAuthorizationNeeded);
}

TEST_F(PrintingOAuth2AuthorizationZoneTest, CancellationDuringInitialization) {
  CreateAuthorizationZone("clientID");
  CallbackResult cr_0;
  CallbackResult cr_1;

  // Try to start two sessions and cancel before the first response from the
  // server returns.
  authorization_zone_->InitAuthorization("scope0", BindResult(cr_0));
  authorization_zone_->InitAuthorization("scope1", BindResult(cr_1));
  authorization_zone_->MarkAuthorizationZoneAsUntrusted();
  EXPECT_EQ(cr_0.status, StatusCode::kUntrustedAuthorizationServer);
  EXPECT_EQ(cr_1.status, StatusCode::kUntrustedAuthorizationServer);

  // Response from the server should not trigger anything.
  ProcessMetadataRequest();
  EXPECT_EQ(cr_0.status, StatusCode::kUntrustedAuthorizationServer);
  EXPECT_EQ(cr_1.status, StatusCode::kUntrustedAuthorizationServer);
}

TEST_F(PrintingOAuth2AuthorizationZoneTest, CancelExistingSessions) {
  CreateAuthorizationZone("clientID");
  CallbackResult cr_0;
  CallbackResult cr_1;

  // Start two sessions and send request for the first access token.
  authorization_zone_->InitAuthorization("scope0", BindResult(cr_0));
  authorization_zone_->InitAuthorization("scope1", BindResult(cr_1));
  ProcessMetadataRequest();
  ASSERT_EQ(cr_0.status, printing::oauth2::StatusCode::kOK);
  ASSERT_EQ(cr_1.status, printing::oauth2::StatusCode::kOK);
  auto auth_url_0 = SimulateAuthorization(cr_0.data, "auth_code_0", "scope0");
  auto auth_url_1 = SimulateAuthorization(cr_1.data, "auth_code_1", "scope1");
  authorization_zone_->FinishAuthorization(GURL(auth_url_0), BindResult(cr_0));
  authorization_zone_->FinishAuthorization(GURL(auth_url_1), BindResult(cr_1));

  // Get the access token for the first session and ask for an endpoint access
  // token.
  ProcessFirstTokenRequest("auth_code_0", "acc_token_0", "ref_token_0");
  ASSERT_EQ(cr_0.status, printing::oauth2::StatusCode::kOK);
  authorization_zone_->GetEndpointAccessToken(chromeos::Uri("ipp://printer0"),
                                              "scope0", BindResult(cr_0));

  // Cancel the zone. All pending callbacks should return.
  authorization_zone_->MarkAuthorizationZoneAsUntrusted();
  EXPECT_EQ(cr_0.status, StatusCode::kUntrustedAuthorizationServer);
  EXPECT_EQ(cr_1.status, StatusCode::kUntrustedAuthorizationServer);
}

}  // namespace
}  // namespace oauth2
}  // namespace printing
}  // namespace ash
