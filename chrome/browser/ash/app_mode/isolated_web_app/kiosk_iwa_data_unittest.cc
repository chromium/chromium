// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/isolated_web_app/kiosk_iwa_data.h"

#include <optional>
#include <string>
#include <string_view>

#include "base/check.h"
#include "base/path_service.h"
#include "base/version.h"
#include "chrome/browser/ash/app_mode/kiosk_app_data_delegate.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "chromeos/ash/components/policy/device_local_account/device_local_account_type.h"
#include "components/prefs/testing_pref_service.h"
#include "components/webapps/isolated_web_apps/update_channel.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace ash {

namespace {
constexpr char kTestAccount[] = "test_account1";
constexpr char kTestWebBundleId[] =
    "aerugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaic";
constexpr char kTestUpdateUrl[] = "https://iwa.com/path/update.json";
constexpr char kTestUpdateChannel[] = "channel_name";
constexpr char kTestPinnedVersion[] = "0.1.2";

constexpr bool kAllowDowngrades = true;
constexpr bool kDisallowDowngrades = false;

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
  static policy::IsolatedWebAppKioskBasicInfo CreateTestPolicyInfo(
      const std::string& web_bundle_id = kTestWebBundleId,
      const std::string& update_manifest_url = kTestUpdateUrl,
      const std::string& update_channel = kTestUpdateChannel,
      const std::string& pinned_version = kTestPinnedVersion,
      bool allow_downgrades = kDisallowDowngrades) {
    return {web_bundle_id, update_manifest_url, update_channel, pinned_version,
            allow_downgrades};
  }
  FakeKioskAppDataDelegate delegate_;
  TestingPrefServiceSimple local_state_;
};

TEST_F(KioskIwaDataTest, CreateFailWithEmptyBundleId) {
  constexpr char kEmptyId[] = "";
  auto iwa_data = KioskIwaData::Create(
      GetTestUserId(), CreateTestPolicyInfo(kEmptyId), delegate_, local_state_);
  EXPECT_EQ(iwa_data, nullptr);
}

TEST_F(KioskIwaDataTest, CreateFailWithBadBundleId) {
  constexpr char kBadWebBundleId[] = "abcd";
  auto iwa_data = KioskIwaData::Create(GetTestUserId(),
                                       CreateTestPolicyInfo(kBadWebBundleId),
                                       delegate_, local_state_);
  EXPECT_EQ(iwa_data, nullptr);
}

TEST_F(KioskIwaDataTest, CreateFailWithEmptyUrl) {
  constexpr char kEmptyUrl[] = "";
  auto iwa_data = KioskIwaData::Create(
      GetTestUserId(), CreateTestPolicyInfo(kTestWebBundleId, kEmptyUrl),
      delegate_, local_state_);
  EXPECT_EQ(iwa_data, nullptr);
}

TEST_F(KioskIwaDataTest, CreateFailWithBadUrl) {
  constexpr char kBadUrl[] = "http:://update.json";
  auto iwa_data = KioskIwaData::Create(
      GetTestUserId(), CreateTestPolicyInfo(kTestWebBundleId, kBadUrl),
      delegate_, local_state_);
  EXPECT_EQ(iwa_data, nullptr);
}

TEST_F(KioskIwaDataTest, CreateSuccessWithEmptyUpdateChannel) {
  constexpr char kEmptyUpdateChannel[] = "";
  auto iwa_data = KioskIwaData::Create(
      GetTestUserId(),
      CreateTestPolicyInfo(kTestWebBundleId, kTestUpdateUrl,
                           kEmptyUpdateChannel),
      delegate_, local_state_);
  ASSERT_NE(iwa_data, nullptr);
  EXPECT_EQ(iwa_data->update_channel(),
            web_app::UpdateChannel::default_channel());
}

TEST_F(KioskIwaDataTest, CreateFailWithBadChannel) {
  constexpr char kNonUtf8[] = "\xC0\xC1";
  auto iwa_data = KioskIwaData::Create(
      GetTestUserId(),
      CreateTestPolicyInfo(kTestWebBundleId, kTestUpdateUrl, kNonUtf8),
      delegate_, local_state_);
  EXPECT_EQ(iwa_data, nullptr);
}

TEST_F(KioskIwaDataTest, CreateSuccessWithNoPinnedVersion) {
  constexpr char kEmptyPinnedVersion[] = "";
  auto iwa_data = KioskIwaData::Create(
      GetTestUserId(),
      CreateTestPolicyInfo(kTestWebBundleId, kTestUpdateUrl, kTestUpdateChannel,
                           kEmptyPinnedVersion),
      delegate_, local_state_);
  ASSERT_NE(iwa_data, nullptr);
  EXPECT_EQ(iwa_data->pinned_version(), std::nullopt);
}

TEST_F(KioskIwaDataTest, CreateFailWithBadPinnedVersion) {
  constexpr char kBadVersion[] = "not_a_version";
  auto iwa_data = KioskIwaData::Create(
      GetTestUserId(),
      CreateTestPolicyInfo(kTestWebBundleId, kTestUpdateUrl, kTestUpdateChannel,
                           kBadVersion),
      delegate_, local_state_);
  EXPECT_EQ(iwa_data, nullptr);
}

TEST_F(KioskIwaDataTest, CreateFailWithDowngradeAndNoPinnedVersion) {
  constexpr char kEmptyPinnedVersion[] = "";
  auto iwa_data = KioskIwaData::Create(
      GetTestUserId(),
      CreateTestPolicyInfo(kTestWebBundleId, kTestUpdateUrl, kTestUpdateChannel,
                           kEmptyPinnedVersion, kAllowDowngrades),
      delegate_, local_state_);
  EXPECT_EQ(iwa_data, nullptr);
}

TEST_F(KioskIwaDataTest, CreateSuccessWithPinningDowngradeAndNoChannel) {
  constexpr char kEmptyUpdateChannel[] = "";
  auto iwa_data = KioskIwaData::Create(
      GetTestUserId(),
      CreateTestPolicyInfo(kTestWebBundleId, kTestUpdateUrl,
                           kEmptyUpdateChannel, kTestPinnedVersion,
                           kAllowDowngrades),
      delegate_, local_state_);
  ASSERT_NE(iwa_data, nullptr);
  EXPECT_EQ(iwa_data->update_channel(),
            web_app::UpdateChannel::default_channel());
  EXPECT_EQ(iwa_data->pinned_version(), base::Version(kTestPinnedVersion));
  EXPECT_EQ(iwa_data->allow_downgrades(), kAllowDowngrades);
}

TEST_F(KioskIwaDataTest, CreateSuccessWithAllValues) {
  const auto kExpectedOrigin = url::Origin::CreateFromNormalizedTuple(
      chrome::kIsolatedAppScheme, kTestWebBundleId, 0);
  const auto kExpectedWebAppId =
      web_app::GenerateAppId("", kExpectedOrigin.GetURL());
  const std::string kExpectedDefaultName = "iwa.com/path/";
  const auto kExpectedChannel =
      web_app::UpdateChannel::Create(kTestUpdateChannel);
  const auto kExpectedPinnedVersion = base::Version(kTestPinnedVersion);

  auto iwa_data = KioskIwaData::Create(
      GetTestUserId(),
      CreateTestPolicyInfo(kTestWebBundleId, kTestUpdateUrl, kTestUpdateChannel,
                           kTestPinnedVersion, kAllowDowngrades),
      delegate_, local_state_);
  ASSERT_NE(iwa_data, nullptr);

  EXPECT_EQ(iwa_data->origin(), kExpectedOrigin);
  EXPECT_EQ(iwa_data->app_id(), kExpectedWebAppId);
  EXPECT_EQ(iwa_data->name(), kExpectedDefaultName);

  EXPECT_EQ(iwa_data->web_bundle_id().id(), kTestWebBundleId);
  EXPECT_EQ(iwa_data->update_manifest_url().spec(), kTestUpdateUrl);
  EXPECT_EQ(iwa_data->update_channel(), kExpectedChannel);
  EXPECT_EQ(iwa_data->pinned_version(), kExpectedPinnedVersion);
  EXPECT_EQ(iwa_data->allow_downgrades(), kAllowDowngrades);
}

}  // namespace ash
