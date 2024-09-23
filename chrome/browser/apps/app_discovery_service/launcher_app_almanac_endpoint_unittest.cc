// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_discovery_service/launcher_app_almanac_endpoint.h"

#include <optional>

#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/almanac_api_client/device_info_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {
namespace {

class LauncherAppAlmanacEndpointTest : public testing::Test {
 public:
  void SetUp() override {
    TestingProfile::Builder profile_builder;
    profile_builder.SetSharedURLLoaderFactory(
        url_loader_factory_.GetSafeWeakWrapper());
    profile_ = profile_builder.Build();
  }

  Profile* profile() { return profile_.get(); }

 protected:
  network::TestURLLoaderFactory url_loader_factory_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  ash::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(LauncherAppAlmanacEndpointTest, GetAppsSuccess) {
  proto::LauncherAppResponse response;
  response.add_app_groups();

  url_loader_factory_.AddResponse(
      launcher_app_almanac_endpoint::GetServerUrl().spec(),
      response.SerializeAsString());

  base::test::TestFuture<std::optional<proto::LauncherAppResponse>>
      observed_response;
  launcher_app_almanac_endpoint::GetApps(profile(),
                                         observed_response.GetCallback());
  ASSERT_TRUE(observed_response.Get().has_value());
  EXPECT_EQ(observed_response.Get()->app_groups_size(), 1);
}

TEST_F(LauncherAppAlmanacEndpointTest, GetAppsEmptyResponse) {
  url_loader_factory_.AddResponse(
      launcher_app_almanac_endpoint::GetServerUrl().spec(), "");
  base::test::TestFuture<std::optional<proto::LauncherAppResponse>> response;
  launcher_app_almanac_endpoint::GetApps(profile(), response.GetCallback());
  ASSERT_TRUE(response.Get().has_value());
  EXPECT_EQ(response.Get()->app_groups_size(), 0);
}

TEST_F(LauncherAppAlmanacEndpointTest, GetAppsError) {
  url_loader_factory_.AddResponse(
      launcher_app_almanac_endpoint::GetServerUrl().spec(),
      /*content=*/"", net::HTTP_INTERNAL_SERVER_ERROR);

  base::test::TestFuture<std::optional<proto::LauncherAppResponse>> response;
  launcher_app_almanac_endpoint::GetApps(profile(), response.GetCallback());
  EXPECT_FALSE(response.Get().has_value());
}

TEST_F(LauncherAppAlmanacEndpointTest, GetAppsNetworkError) {
  url_loader_factory_.AddResponse(
      launcher_app_almanac_endpoint::GetServerUrl(),
      network::mojom::URLResponseHead::New(),
      /*content=*/"",
      network::URLLoaderCompletionStatus(net::ERR_INSUFFICIENT_RESOURCES));

  base::test::TestFuture<std::optional<proto::LauncherAppResponse>> response;
  launcher_app_almanac_endpoint::GetApps(profile(), response.GetCallback());
  EXPECT_FALSE(response.Get().has_value());
}

}  // namespace
}  // namespace apps
