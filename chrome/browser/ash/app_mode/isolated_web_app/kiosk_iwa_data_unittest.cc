// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/isolated_web_app/kiosk_iwa_data.h"

#include <string>
#include <string_view>

#include "base/check.h"
#include "base/path_service.h"
#include "chrome/browser/ash/app_mode/kiosk_app_data_delegate.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/common/chrome_paths.h"
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

class FakeKioskAppDataDelegate : public KioskAppDataDelegate {
 public:
  FakeKioskAppDataDelegate() = default;
  FakeKioskAppDataDelegate(const FakeKioskAppDataDelegate&) = delete;
  FakeKioskAppDataDelegate& operator=(const FakeKioskAppDataDelegate&) = delete;
  ~FakeKioskAppDataDelegate() override = default;

 private:
  // KioskAppDataDelegate:
  base::FilePath GetKioskAppIconCacheDir() override {
    base::FilePath user_data_dir;
    bool has_dir =
        base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
    CHECK(has_dir);
    return user_data_dir;
  }

  void OnKioskAppDataChanged(const std::string& app_id) override {}

  void OnKioskAppDataLoadFailure(const std::string& app_id) override {}

  void OnExternalCacheDamaged(const std::string& app_id) override {}
};
}  // namespace

class KioskIwaDataTest : public testing::Test {
 protected:
  FakeKioskAppDataDelegate delegate_;
};

TEST_F(KioskIwaDataTest, CreateFailWithEmptyBundleId) {
  constexpr char kEmptyId[] = "";
  auto iwa_data = KioskIwaData::Create(GetTestUserId(), kEmptyId,
                                       GURL(kTestUpdateUrl), delegate_);
  EXPECT_EQ(iwa_data, nullptr);
}

TEST_F(KioskIwaDataTest, CreateFailWithBadBundleId) {
  constexpr char kBadWebBundleId[] = "abcd";
  auto iwa_data = KioskIwaData::Create(GetTestUserId(), kBadWebBundleId,
                                       GURL(kTestUpdateUrl), delegate_);
  EXPECT_EQ(iwa_data, nullptr);
}

TEST_F(KioskIwaDataTest, CreateFailWithEmptyUrl) {
  const GURL kEmptyUrl;
  auto iwa_data = KioskIwaData::Create(GetTestUserId(), kTestWebBundleId,
                                       kEmptyUrl, delegate_);
  EXPECT_EQ(iwa_data, nullptr);
}

TEST_F(KioskIwaDataTest, CreateFailWithBadUrl) {
  const GURL kBadUrl("http:://update.json");
  auto iwa_data = KioskIwaData::Create(GetTestUserId(), kTestWebBundleId,
                                       kBadUrl, delegate_);
  EXPECT_EQ(iwa_data, nullptr);
}

TEST_F(KioskIwaDataTest, CreateSuccess) {
  const auto kExpectedOrigin = url::Origin::CreateFromNormalizedTuple(
      chrome::kIsolatedAppScheme, kTestWebBundleId, 0);
  const auto kExpectedWebAppId =
      web_app::GenerateAppId("", kExpectedOrigin.GetURL());

  auto iwa_data = KioskIwaData::Create(GetTestUserId(), kTestWebBundleId,
                                       GURL(kTestUpdateUrl), delegate_);
  ASSERT_NE(iwa_data, nullptr);
  EXPECT_EQ(iwa_data->origin(), kExpectedOrigin);
  EXPECT_EQ(iwa_data->app_id(), kExpectedWebAppId);
  EXPECT_EQ(iwa_data->web_bundle_id().id(), kTestWebBundleId);
  EXPECT_EQ(iwa_data->update_manifest_url().spec(), kTestUpdateUrl);
}

}  // namespace ash
