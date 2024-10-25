// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/isolated_web_app/kiosk_iwa_data.h"

#include <string>
#include <string_view>

#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/common/url_constants.h"
#include "components/policy/core/common/device_local_account_type.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace ash {

namespace {
constexpr char kTestAccount[] = "test_account1";
constexpr char kTestWebBundleId[] =
    "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic";
constexpr char kTestUpdateUrl[] = "https://example.com/update.json";

std::string GetTestUserId() {
  return policy::GenerateDeviceLocalAccountUserId(
      kTestAccount, policy::DeviceLocalAccountType::kKioskIsolatedWebApp);
}
}  // namespace

using KioskIwaDataTest = ::testing::Test;

TEST_F(KioskIwaDataTest, CreateFailWithEmptyBundleId) {
  constexpr char kEmptyId[] = "";
  auto iwa_data =
      KioskIwaData::Create(GetTestUserId(), kEmptyId, GURL(kTestUpdateUrl));
  EXPECT_EQ(iwa_data, nullptr);
}

TEST_F(KioskIwaDataTest, CreateFailWithBadBundleId) {
  constexpr char kBadWebBundleId[] = "abcd";
  auto iwa_data = KioskIwaData::Create(GetTestUserId(), kBadWebBundleId,
                                       GURL(kTestUpdateUrl));
  EXPECT_EQ(iwa_data, nullptr);
}

TEST_F(KioskIwaDataTest, CreateFailWithEmptyUrl) {
  const GURL kEmptyUrl;
  auto iwa_data =
      KioskIwaData::Create(GetTestUserId(), kTestWebBundleId, kEmptyUrl);
  EXPECT_EQ(iwa_data, nullptr);
}

TEST_F(KioskIwaDataTest, CreateFailWithBadUrl) {
  const GURL kBadUrl("http:://update.json");
  auto iwa_data =
      KioskIwaData::Create(GetTestUserId(), kTestWebBundleId, kBadUrl);
  EXPECT_EQ(iwa_data, nullptr);
}

TEST_F(KioskIwaDataTest, CreateSuccess) {
  const auto kExpectedOrigin = url::Origin::CreateFromNormalizedTuple(
      chrome::kIsolatedAppScheme, kTestWebBundleId, 0);
  const auto kExpectedWebAppId =
      web_app::GenerateAppId("", kExpectedOrigin.GetURL());

  auto iwa_data = KioskIwaData::Create(GetTestUserId(), kTestWebBundleId,
                                       GURL(kTestUpdateUrl));
  ASSERT_NE(iwa_data, nullptr);
  EXPECT_EQ(iwa_data->origin(), kExpectedOrigin);
  EXPECT_EQ(iwa_data->app_id(), kExpectedWebAppId);
  EXPECT_EQ(iwa_data->web_bundle_id().id(), kTestWebBundleId);
  EXPECT_EQ(iwa_data->update_manifest_url().spec(), kTestUpdateUrl);
}

}  // namespace ash
