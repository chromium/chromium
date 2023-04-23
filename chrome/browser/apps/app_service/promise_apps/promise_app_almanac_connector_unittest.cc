// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/promise_apps/promise_app_almanac_connector.h"

#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/app_service/package_id.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_wrapper.h"
#include "chrome/browser/apps/app_service/promise_apps/proto/promise_app.pb.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace apps {

const PackageId kTestPackageId(AppType::kArc, "test.package.name");

class PromiseAppAlmanacConnectorTest : public testing::Test {
 public:
  PromiseAppAlmanacConnector* connector() { return connector_.get(); }

  void SetUp() override {
    url_loader_factory_ = std::make_unique<network::TestURLLoaderFactory>();
    testing::Test::SetUp();
    TestingProfile::Builder profile_builder;
    profile_builder.SetSharedURLLoaderFactory(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            url_loader_factory_.get()));
    profile_ = profile_builder.Build();
    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            url_loader_factory_.get());
    connector_ = std::make_unique<PromiseAppAlmanacConnector>(profile_.get());
  }

  network::TestURLLoaderFactory* url_loader_factory() {
    return url_loader_factory_.get();
  }

  Profile* profile() { return profile_.get(); }

 private:
  std::unique_ptr<network::TestURLLoaderFactory> url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  std::unique_ptr<PromiseAppAlmanacConnector> connector_;
  content::BrowserTaskEnvironment task_environment_;
  ash::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
  std::unique_ptr<Profile> profile_;
};

TEST_F(PromiseAppAlmanacConnectorTest, GetPromiseAppInfoRequest) {
  std::string method;
  std::string method_override_header;
  std::string content_type;
  std::string body;

  url_loader_factory()->SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        request.headers.GetHeader(net::HttpRequestHeaders::kContentType,
                                  &content_type);
        request.headers.GetHeader("X-HTTP-Method-Override",
                                  &method_override_header);
        method = request.method;
        body = network::GetUploadData(request);
      }));
  url_loader_factory()->AddResponse(
      PromiseAppAlmanacConnector::GetServerUrl().spec(), /*content=*/"");

  base::test::TestFuture<absl::optional<PromiseAppWrapper>> test_callback;
  connector()->GetPromiseAppInfo(kTestPackageId, test_callback.GetCallback());
  EXPECT_TRUE(test_callback.Wait());

  EXPECT_EQ(method, "POST");
  EXPECT_EQ(method_override_header, "GET");
  EXPECT_EQ(content_type, "application/x-protobuf");

  proto::PromiseAppRequest request;
  ASSERT_TRUE(request.ParseFromString(body));
  EXPECT_EQ(request.package_id(), "android:test.package.name");
}

TEST_F(PromiseAppAlmanacConnectorTest, GetPromiseAppInfoSuccessResponse) {
  proto::PromiseAppResponse response;
  response.set_package_id(kTestPackageId.ToString());

  url_loader_factory()->AddResponse(
      PromiseAppAlmanacConnector::GetServerUrl().spec(),
      response.SerializeAsString());

  base::test::TestFuture<absl::optional<PromiseAppWrapper>> test_callback;
  connector()->GetPromiseAppInfo(kTestPackageId, test_callback.GetCallback());
  auto promise_app_info = test_callback.Get();

  EXPECT_EQ(promise_app_info->GetPackageId(), kTestPackageId);
}

TEST_F(PromiseAppAlmanacConnectorTest, GetPromiseAppInfoErrorResponse) {
  url_loader_factory()->AddResponse(
      PromiseAppAlmanacConnector::GetServerUrl().spec(), /*content=*/"",
      net::HTTP_INTERNAL_SERVER_ERROR);

  base::test::TestFuture<absl::optional<PromiseAppWrapper>> test_callback;
  connector()->GetPromiseAppInfo(kTestPackageId, test_callback.GetCallback());
  auto promise_app_info = test_callback.Get();

  EXPECT_FALSE(promise_app_info.has_value());
}

}  // namespace apps
