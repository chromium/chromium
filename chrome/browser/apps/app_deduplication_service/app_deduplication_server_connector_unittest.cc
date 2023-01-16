// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_deduplication_service/app_deduplication_server_connector.h"

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/app_deduplication_service/proto/app_deduplication.pb.h"
#include "chrome/browser/apps/app_deduplication_service/proto/deduplication_data.pb.h"
#include "components/version_info/channel.h"
#include "content/public/test/browser_task_environment.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace apps {

class AppDeduplicationServerConnectorTest : public testing::Test {
 public:
  AppDeduplicationServerConnectorTest()
      : test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &url_loader_factory_)) {}

 protected:
  network::TestURLLoaderFactory url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;

  AppDeduplicationServerConnector server_connector_;

 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(AppDeduplicationServerConnectorTest,
       GetDeduplicateAppsFromServerRequest) {
  std::string method;
  std::string method_override_header;
  std::string content_type;

  url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        request.headers.GetHeader(net::HttpRequestHeaders::kContentType,
                                  &content_type);
        request.headers.GetHeader("X-HTTP-Method-Override",
                                  &method_override_header);
        method = request.method;
      }));

  server_connector_.GetDeduplicateAppsFromServer(test_shared_loader_factory_,
                                                 base::DoNothing());

  EXPECT_EQ(method, "POST");
  EXPECT_EQ(method_override_header, "GET");
  EXPECT_EQ(content_type, "application/x-protobuf");
}

TEST_F(AppDeduplicationServerConnectorTest,
       GetDeduplicateAppsFromServerSuccess) {
  proto::DeduplicateResponse response;
  auto* app_group = response.add_app_group();
  auto* app = app_group->add_app();
  app->set_app_id("com.skype.raider");
  app->set_platform("phonehub");

  url_loader_factory_.AddResponse(
      AppDeduplicationServerConnector::GetServerUrl().spec(),
      response.SerializeAsString());

  base::test::TestFuture<absl::optional<proto::DeduplicateData>> test_callback;
  server_connector_.GetDeduplicateAppsFromServer(test_shared_loader_factory_,
                                                 test_callback.GetCallback());
  auto observed_response = test_callback.Get();
  EXPECT_TRUE(observed_response.has_value());
  EXPECT_EQ(observed_response->app_group_size(), 1);
}

TEST_F(AppDeduplicationServerConnectorTest,
       GetDeduplicateAppsFromServerEmptyResponse) {
  url_loader_factory_.AddResponse(
      AppDeduplicationServerConnector::GetServerUrl().spec(), "");

  base::test::TestFuture<absl::optional<proto::DeduplicateData>> response;
  server_connector_.GetDeduplicateAppsFromServer(test_shared_loader_factory_,
                                                 response.GetCallback());
  EXPECT_FALSE(response.Get().has_value());
}

TEST_F(AppDeduplicationServerConnectorTest, GetDeduplicateAppsFromServerError) {
  url_loader_factory_.AddResponse(
      AppDeduplicationServerConnector::GetServerUrl().spec(), /*content=*/"",
      net::HTTP_INTERNAL_SERVER_ERROR);

  base::test::TestFuture<absl::optional<proto::DeduplicateData>> response;
  server_connector_.GetDeduplicateAppsFromServer(test_shared_loader_factory_,
                                                 response.GetCallback());
  EXPECT_FALSE(response.Get().has_value());
}

TEST_F(AppDeduplicationServerConnectorTest,
       GetDeduplicateAppsFromServerNetworkError) {
  url_loader_factory_.AddResponse(
      AppDeduplicationServerConnector::GetServerUrl(),
      network::mojom::URLResponseHead::New(), /*content=*/"",
      network::URLLoaderCompletionStatus(net::ERR_INSUFFICIENT_RESOURCES));

  base::test::TestFuture<absl::optional<proto::DeduplicateData>> response;
  server_connector_.GetDeduplicateAppsFromServer(test_shared_loader_factory_,
                                                 response.GetCallback());
  EXPECT_FALSE(response.Get().has_value());
}

}  // namespace apps
