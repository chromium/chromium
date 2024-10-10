// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/mall/mall_url.h"

#include "base/base64.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/almanac_api_client/device_info_manager.h"
#include "chrome/browser/apps/almanac_api_client/device_info_manager_factory.h"
#include "chrome/browser/apps/almanac_api_client/proto/client_context.pb.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/constants/url_constants.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/url_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash {

class MallUrlTest : public testing::Test {
 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(MallUrlTest, GetMallLaunchUrl) {
  TestingProfile profile;

  ash::system::ScopedFakeStatisticsProvider fake_statistics_provider;
  fake_statistics_provider.SetMachineStatistic(ash::system::kHardwareClassKey,
                                               "SHIBA D0G-F4N-C1UB");

  base::test::TestFuture<apps::DeviceInfo> device_info;
  apps::DeviceInfoManager* manager =
      apps::DeviceInfoManagerFactory::GetForProfile(&profile);

  manager->GetDeviceInfo(device_info.GetCallback());

  GURL launch_url = GetMallLaunchUrl(device_info.Get());

  ASSERT_EQ(launch_url.host(), GURL(chromeos::kAppMallBaseUrl).host());

  std::vector<std::string> query_parts = base::SplitString(
      launch_url.query(), "=", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);

  ASSERT_EQ(query_parts[0], "context");

  std::string proto_string;
  ASSERT_TRUE(base::Base64Decode(net::UnescapePercentEncodedUrl(query_parts[1]),
                                 &proto_string,
                                 base::Base64DecodePolicy::kStrict));

  apps::proto::ClientContext decoded_context;
  ASSERT_TRUE(decoded_context.ParseFromString(proto_string));
  ASSERT_EQ(decoded_context.device_context().hardware_id(),
            "SHIBA D0G-F4N-C1UB");
}

TEST_F(MallUrlTest, GetMallLaunchUrl_BasePath) {
  EXPECT_THAT(GetMallLaunchUrl(apps::DeviceInfo(), "/").spec(),
              testing::StartsWith("https://discover.apps.chrome/?context="));
}

TEST_F(MallUrlTest, GetMallLaunchUrl_SimplePath) {
  EXPECT_THAT(
      GetMallLaunchUrl(apps::DeviceInfo(), "/apps/").spec(),
      testing::StartsWith("https://discover.apps.chrome/apps/?context="));
}

TEST_F(MallUrlTest, GetMallLaunchUrl_PathWithQuery) {
  // Query-looking parts of the path parameter should not be interpreted as
  // query parameters.
  EXPECT_THAT(GetMallLaunchUrl(apps::DeviceInfo(), "/search?q=netflix").spec(),
              testing::StartsWith(
                  "https://discover.apps.chrome/search%3Fq=netflix?context="));
}

TEST_F(MallUrlTest, GetMallLaunchUrl_RelativePath) {
  EXPECT_THAT(
      GetMallLaunchUrl(apps::DeviceInfo(), "../apps/").spec(),
      testing::StartsWith("https://discover.apps.chrome/apps/?context="));
}

TEST_F(MallUrlTest, GetMallLaunchUrl_RelativePathHost) {
  EXPECT_THAT(GetMallLaunchUrl(apps::DeviceInfo(), "//example.com/").spec(),
              testing::StartsWith(
                  "https://discover.apps.chrome//example.com/?context="));
}

}  // namespace ash
