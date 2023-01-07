// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/oauth2/test_authorization_server.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash {
namespace printing {
namespace oauth2 {
namespace {

// Helper function that moves the content of `response_body` to `target`.
void SavePayload(std::string* target,
                 std::unique_ptr<std::string> response_body) {
  CHECK(target);
  if (response_body) {
    *target = std::move(*response_body);
  } else {
    target->clear();
  }
}

// Helper function to retrieve HTTP status from `url_loader`.
// Returns -1 when it is not possible.
int GetHttpStatus(network::SimpleURLLoader* url_loader) {
  if (url_loader && url_loader->ResponseInfo() &&
      url_loader->ResponseInfo()->headers) {
    return url_loader->ResponseInfo()->headers->response_code();
  }
  return -1;
}

TEST(PrintingOAuth2TestAuthorizationServerTest, ParseURLParameters) {
  base::flat_map<std::string, std::string> results;
  results["trash"] = "something";
  EXPECT_TRUE(ParseURLParameters("ala=ma&kota", results));
  EXPECT_EQ(results.size(), 2u);
  EXPECT_TRUE(results.contains("kota"));
  EXPECT_EQ(results["ala"], "ma");
  EXPECT_EQ(results["kota"], "");
}

TEST(PrintingOAuth2TestAuthorizationServerTest, BuildMetadata) {
  auto data = BuildMetadata("server_uri", "auth", "token", "reg", "rev");
  EXPECT_EQ(*data.FindString("issuer"), "server_uri");
  EXPECT_EQ(*data.FindString("authorization_endpoint"), "auth");
  EXPECT_EQ(*data.FindString("token_endpoint"), "token");
  EXPECT_EQ(*data.FindString("registration_endpoint"), "reg");
  EXPECT_EQ(*data.FindString("revocation_endpoint"), "rev");
  ASSERT_TRUE(data.FindList("response_types_supported"));
  ASSERT_EQ(data.FindList("response_types_supported")->size(), 1u);
  EXPECT_EQ(data.FindList("response_types_supported")->front().GetString(),
            "code");
}

TEST(PrintingOAuth2TestAuthorizationServerTest, ReceiveGETAndResponse) {
  FakeAuthorizationServer server;

  // Prepare SimpleURLLoader and send the request.
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->method = "GET";
  resource_request->url = GURL("https://abc/def");
  auto url_loader = network::SimpleURLLoader::Create(
      std::move(resource_request), TRAFFIC_ANNOTATION_FOR_TESTS);
  std::string response_payload;
  url_loader->DownloadToString(server.GetURLLoaderFactory().get(),
                               base::BindOnce(&SavePayload, &response_payload),
                               1024);

  // Process and check the request and send the response.
  ASSERT_EQ(server.ReceiveGET("https://abc/def"), "");
  base::Value::Dict content;
  server.ResponseWithJSON(net::HttpStatusCode::HTTP_CREATED, content);

  // Check the response.
  EXPECT_EQ(response_payload, "{}");
  EXPECT_EQ(GetHttpStatus(url_loader.get()),
            static_cast<int>(net::HttpStatusCode::HTTP_CREATED));
}

TEST(PrintingOAuth2TestAuthorizationServerTest,
     ReceivePOSTWithJSONAndResponse) {
  FakeAuthorizationServer server;

  // Prepare SimpleURLLoader and send the request.
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->method = "POST";
  resource_request->url = GURL("https://abc/def");
  auto url_loader = network::SimpleURLLoader::Create(
      std::move(resource_request), TRAFFIC_ANNOTATION_FOR_TESTS);
  url_loader->AttachStringForUpload(R"({ "field1": "val1", "field2":"val2"})",
                                    "application/json");
  std::string response_payload;
  url_loader->DownloadToString(server.GetURLLoaderFactory().get(),
                               base::BindOnce(&SavePayload, &response_payload),
                               1024);

  // Process and check the request and send the response.
  base::Value::Dict content;
  ASSERT_EQ(server.ReceivePOSTWithJSON("https://abc/def", content), "");
  EXPECT_EQ(content.size(), 2u);
  EXPECT_EQ(*content.FindString("field1"), "val1");
  EXPECT_EQ(*content.FindString("field2"), "val2");
  content.Set("field3", "val3");
  server.ResponseWithJSON(net::HttpStatusCode::HTTP_OK, content);

  // Check the response.
  EXPECT_EQ(GetHttpStatus(url_loader.get()),
            static_cast<int>(net::HttpStatusCode::HTTP_OK));
  base::Value parsed = base::test::ParseJson(response_payload);
  ASSERT_TRUE(parsed.is_dict());
  EXPECT_EQ(parsed, base::Value(std::move(content)));
}

TEST(PrintingOAuth2TestAuthorizationServerTest,
     ReceivePOSTWithURLParamsAndError) {
  FakeAuthorizationServer server;

  // Prepare SimpleURLLoader and send the request.
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->method = "PUT";  // wrong method
  resource_request->url = GURL("https://abc/def");
  auto url_loader = network::SimpleURLLoader::Create(
      std::move(resource_request), TRAFFIC_ANNOTATION_FOR_TESTS);
  url_loader->AttachStringForUpload("f=v", "application/x-www-form-urlencoded");
  std::string response_payload;
  url_loader->DownloadToString(server.GetURLLoaderFactory().get(),
                               base::BindOnce(&SavePayload, &response_payload),
                               1024);

  // Process the request and check the error message.
  base::flat_map<std::string, std::string> content;
  auto err_msg = server.ReceivePOSTWithURLParams("https://abc/def", content);
  EXPECT_EQ(err_msg, "Invalid HTTP method: got \"PUT\", expected \"POST\";  ");
}

}  // namespace
}  // namespace oauth2
}  // namespace printing
}  // namespace ash
