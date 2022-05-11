// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/oauth2/authorization_zone.h"

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chrome/browser/ash/printing/oauth2/constants.h"
#include "chrome/browser/ash/printing/oauth2/test_authorization_server.h"
#include "chromeos/printing/uri.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
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
    authorization_zone_ = printing::oauth2::AuthorizationZone::Create(
        server_.GetURLLoaderFactory(), auth_server_uri, client_id);
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
    base::flat_map<std::string, base::Value> fields;
    EXPECT_EQ("", server_.ReceivePOSTWithJSON(registration_uri_, fields));
    fields["client_id"] = base::Value(client_id);
    server_.ResponseWithJSON(net::HttpStatusCode::HTTP_CREATED, fields);
  }

 protected:
  const std::string authorization_server_uri_ = "https://example.com/path";
  const std::string metadata_uri_ =
      "https://example.com/.well-known/oauth-authorization-server/path";
  const std::string authorization_uri_ = "https://example.com/authorization";
  const std::string token_uri_ = "https://example.com/token";
  const std::string registration_uri_ = "https://example.com/registration";

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
  const std::string authorization_URL = cr.data;

  // Parse and verify the returned URL.
  auto question_mark = authorization_URL.find('?');
  ASSERT_LT(question_mark, authorization_URL.size());
  const std::string auth_host_path = authorization_URL.substr(0, question_mark);
  base::flat_map<std::string, std::string> params;
  ASSERT_TRUE(
      ParseURLParameters(authorization_URL.substr(question_mark + 1), params));
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
  const std::string authorization_URL = cr.data;

  // Parse and verify the returned URL.
  auto question_mark = authorization_URL.find('?');
  ASSERT_LT(question_mark, authorization_URL.size());
  const std::string auth_host_path = authorization_URL.substr(0, question_mark);
  base::flat_map<std::string, std::string> params;
  ASSERT_TRUE(
      ParseURLParameters(authorization_URL.substr(question_mark + 1), params));
  EXPECT_EQ(params["client_id"], "clientID_!@#$");
  EXPECT_EQ(params.count("scope"), 0);
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

}  // namespace
}  // namespace oauth2
}  // namespace printing
}  // namespace ash
