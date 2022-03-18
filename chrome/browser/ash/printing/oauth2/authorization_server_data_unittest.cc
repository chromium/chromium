// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/oauth2/authorization_server_data.h"

#include <string>

#include "base/containers/flat_map.h"
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

// Helper function that checks if `value_dict` contains a field `name` with
// a single-element list that contains a string equals `value`. It doesn't stop
// execution, just logs errors.
void ExpectOneElementListOfStrings(
    const base::Value::DeprecatedDictStorage& value_dict,
    const std::string& name,
    const std::string& value) {
  auto it = value_dict.find(name);
  ASSERT_NE(it, value_dict.end());
  ASSERT_EQ(it->second.type(), base::Value::Type::LIST);
  auto nodeAsList = base::Value::AsListValue(it->second).GetListDeprecated();
  ASSERT_EQ(nodeAsList.size(), 1);
  ASSERT_TRUE(nodeAsList.front().is_string());
  EXPECT_EQ(nodeAsList.front().GetString(), value);
}

TEST(PrintingOAuth2AuthorizationServerDataTest, InitialState) {
  FakeAuthorizationServer server;
  AuthorizationServerData asd(server.GetURLLoaderFactory(),
                              GURL("https://a.b/c"), "abc");
  EXPECT_EQ(asd.AuthorizationServerURI().spec(), "https://a.b/c");
  EXPECT_EQ(asd.ClientId(), "abc");
  EXPECT_TRUE(asd.AuthorizationEndpointURI().is_empty());
  EXPECT_TRUE(asd.TokenEndpointURI().is_empty());
  EXPECT_TRUE(asd.RegistrationEndpointURI().is_empty());
  EXPECT_TRUE(asd.RevocationEndpointURI().is_empty());
  EXPECT_FALSE(asd.IsReady());
}

TEST(PrintingOAuth2AuthorizationServerDataTest, MetadataRequest) {
  FakeAuthorizationServer server;
  AuthorizationServerData asd(server.GetURLLoaderFactory(),
                              GURL("https://a.b/c"), "abc");
  CallbackResult cr;
  asd.Initialize(BindResult(cr));

  // Simulate processing of Metadata Request (rfc8414, section 3).
  ASSERT_EQ(
      server.ReceiveGET("https://a.b/.well-known/oauth-authorization-server/c"),
      "");
  auto params =
      BuildMetadata("https://a.b/c", "https://a/auth", "https://b/token",
                    "https://c/reg", "https://d/rev");
  server.ResponseWithJSON(net::HttpStatusCode::HTTP_OK, params);

  // Check the callback and the object.
  ASSERT_EQ(cr.status, StatusCode::kOK);
  EXPECT_TRUE(cr.data.empty());
  EXPECT_EQ(asd.AuthorizationServerURI().spec(), "https://a.b/c");
  EXPECT_EQ(asd.ClientId(), "abc");
  EXPECT_EQ(asd.AuthorizationEndpointURI().spec(), "https://a/auth");
  EXPECT_EQ(asd.TokenEndpointURI().spec(), "https://b/token");
  EXPECT_EQ(asd.RegistrationEndpointURI().spec(), "https://c/reg");
  EXPECT_EQ(asd.RevocationEndpointURI().spec(), "https://d/rev");
  EXPECT_TRUE(asd.IsReady());
}

TEST(PrintingOAuth2AuthorizationServerDataTest, RegistrationRequest) {
  FakeAuthorizationServer server;
  AuthorizationServerData asd(server.GetURLLoaderFactory(),
                              GURL("https://a.b/c"), "");
  CallbackResult cr;
  asd.Initialize(BindResult(cr));

  // Simulate processing of Metadata Request (rfc8414, section 3).
  ASSERT_EQ(
      server.ReceiveGET("https://a.b/.well-known/oauth-authorization-server/c"),
      "");
  auto params =
      BuildMetadata("https://a.b/c", "https://a/auth", "https://b/token",
                    "https://c/reg", "https://d/rev");
  server.ResponseWithJSON(net::HttpStatusCode::HTTP_OK, params);

  // Simulate processing of Registration Request (rfc7591, section 3).
  ASSERT_EQ(server.ReceivePOSTWithJSON("https://c/reg", params), "");
  ExpectOneElementListOfStrings(params, "redirect_uris", kRedirectURI);
  ExpectOneElementListOfStrings(params, "token_endpoint_auth_method", "none");
  ExpectOneElementListOfStrings(params, "grant_types", "authorization_code");
  ExpectOneElementListOfStrings(params, "response_types", "code");
  params["client_id"] = base::Value("new_client_id");
  server.ResponseWithJSON(net::HttpStatusCode::HTTP_CREATED, params);

  // Check the callback and the object.
  ASSERT_EQ(cr.status, StatusCode::kOK);
  EXPECT_TRUE(cr.data.empty());
  EXPECT_EQ(asd.AuthorizationServerURI().spec(), "https://a.b/c");
  EXPECT_EQ(asd.ClientId(), "new_client_id");
  EXPECT_EQ(asd.AuthorizationEndpointURI().spec(), "https://a/auth");
  EXPECT_EQ(asd.TokenEndpointURI().spec(), "https://b/token");
  EXPECT_EQ(asd.RegistrationEndpointURI().spec(), "https://c/reg");
  EXPECT_EQ(asd.RevocationEndpointURI().spec(), "https://d/rev");
  EXPECT_TRUE(asd.IsReady());
}

}  // namespace
}  // namespace oauth2
}  // namespace printing
}  // namespace ash
