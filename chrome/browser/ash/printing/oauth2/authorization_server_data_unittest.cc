// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/oauth2/authorization_server_data.h"

#include <string>

#include "base/containers/flat_map.h"
#include "base/values.h"
#include "chrome/browser/ash/printing/oauth2/constants.h"
#include "chrome/browser/ash/printing/oauth2/mock_client_ids_database.h"
#include "chrome/browser/ash/printing/oauth2/status_code.h"
#include "chrome/browser/ash/printing/oauth2/test_authorization_server.h"
#include "net/http/http_status_code.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash {
namespace printing {
namespace oauth2 {
namespace {

// Helper function that checks if `value_dict` contains a field `name` with
// a single-element list that contains a string equals `value`. It doesn't stop
// execution, just logs errors.
void ExpectOneElementListOfStrings(const base::Value::Dict& dict,
                                   const std::string& name,
                                   const std::string& value) {
  const base::Value::List* list = dict.FindList(name);
  ASSERT_TRUE(list);
  ASSERT_EQ(list->size(), 1u);
  ASSERT_TRUE(list->front().is_string());
  EXPECT_EQ(list->front().GetString(), value);
}

TEST(PrintingOAuth2AuthorizationServerDataTest, InitialState) {
  FakeAuthorizationServer server;
  testing::StrictMock<MockClientIdsDatabase> client_ids_database;
  AuthorizationServerData asd(server.GetURLLoaderFactory(),
                              GURL("https://a.b/c"), &client_ids_database);
  EXPECT_EQ(asd.AuthorizationServerURI().spec(), "https://a.b/c");
  EXPECT_EQ(asd.ClientId(), "");
  EXPECT_TRUE(asd.AuthorizationEndpointURI().is_empty());
  EXPECT_TRUE(asd.TokenEndpointURI().is_empty());
  EXPECT_TRUE(asd.RegistrationEndpointURI().is_empty());
  EXPECT_TRUE(asd.RevocationEndpointURI().is_empty());
  EXPECT_FALSE(asd.IsReady());
}

TEST(PrintingOAuth2AuthorizationServerDataTest, MetadataRequest) {
  FakeAuthorizationServer server;
  testing::StrictMock<MockClientIdsDatabase> client_ids_database;
  AuthorizationServerData asd(server.GetURLLoaderFactory(),
                              GURL("https://a.b/c"), &client_ids_database);

  // Simulate fetching client_id from the database.
  EXPECT_CALL(client_ids_database, FetchId)
      .WillOnce([](const GURL& url, StatusCallback callback) {
        EXPECT_EQ(url.spec(), "https://a.b/c");
        std::move(callback).Run(StatusCode::kOK, "abc");
      });

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
  testing::NiceMock<MockClientIdsDatabase> client_ids_database;
  AuthorizationServerData asd(server.GetURLLoaderFactory(),
                              GURL("https://a.b/c"), &client_ids_database);
  CallbackResult cr;
  asd.Initialize(BindResult(cr));

  // Simulate processing of Metadata Request (rfc8414, section 3).
  ASSERT_EQ(
      server.ReceiveGET("https://a.b/.well-known/oauth-authorization-server/c"),
      "");
  base::Value::Dict params =
      BuildMetadata("https://a.b/c", "https://a/auth", "https://b/token",
                    "https://c/reg", "https://d/rev");
  server.ResponseWithJSON(net::HttpStatusCode::HTTP_OK, params);

  // Expect saving the new client id.
  EXPECT_CALL(client_ids_database, StoreId)
      .WillOnce([](const GURL& url, const std::string& id) {
        EXPECT_EQ(url.spec(), "https://a.b/c");
        EXPECT_EQ(id, "new_client_id");
      });

  // Simulate processing of Registration Request (rfc7591, section 3).
  ASSERT_EQ(server.ReceivePOSTWithJSON("https://c/reg", params), "");
  ExpectOneElementListOfStrings(params, "redirect_uris", kRedirectURI);
  ExpectOneElementListOfStrings(params, "token_endpoint_auth_method", "none");
  ExpectOneElementListOfStrings(params, "grant_types", "authorization_code");
  ExpectOneElementListOfStrings(params, "response_types", "code");
  params.Set("client_id", "new_client_id");
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
