// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/almanac_api_client/device_info_manager.h"

#include <memory>
#include <optional>

#include "base/test/scoped_chromeos_version_info.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/almanac_api_client/proto/client_context.pb.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/channel_info.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/channel.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

class DeviceInfoManagerTest : public testing::Test {
 public:
  void SetUp() override {
    device_info_manager_ = base::WrapUnique(new DeviceInfoManager(profile()));
  }

  Profile* profile() { return &profile_; }
  DeviceInfoManager* device_info_manager() {
    return device_info_manager_.get();
  }
  ash::system::FakeStatisticsProvider* statistics_provider() {
    return &fake_statistics_provider_;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;

  std::unique_ptr<DeviceInfoManager> device_info_manager_;
  ash::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
};

TEST_F(DeviceInfoManagerTest, CheckDeviceInfo) {
  const char kLsbRelease[] = R"(
  CHROMEOS_RELEASE_VERSION=123.4.5
  CHROMEOS_RELEASE_BOARD=puff-signed-mp-v11keys
  CHROMEOS_ARC_ANDROID_SDK_VERSION=33
  )";
  base::test::ScopedChromeOSVersionInfo version(kLsbRelease, base::Time());

  statistics_provider()->SetMachineStatistic(ash::system::kHardwareClassKey,
                                             "FOOBAR D0G-F4N-C1UB");

  static constexpr char kTestLocale[] = "test_locale";
  profile()->GetPrefs()->SetString(language::prefs::kApplicationLocale,
                                   kTestLocale);

  base::test::TestFuture<DeviceInfo> info_future;
  device_info_manager()->GetDeviceInfo(info_future.GetCallback());

  DeviceInfo device_info = info_future.Take();

  ASSERT_EQ(device_info.board, "puff");
  ASSERT_FALSE(device_info.model.empty());
  ASSERT_EQ(device_info.user_type, "unmanaged");
  ASSERT_FALSE(device_info.version_info.ash_chrome.empty());
  ASSERT_EQ(device_info.version_info.platform, "123.4.5");
  ASSERT_EQ(device_info.version_info.channel, chrome::GetChannel());
  ASSERT_EQ(device_info.version_info.arc_sdk, 0);
  ASSERT_EQ(device_info.hardware_id, "FOOBAR D0G-F4N-C1UB");
  ASSERT_EQ(device_info.locale, kTestLocale);
  ASSERT_EQ(device_info.custom_label_tag, std::nullopt);
}

TEST_F(DeviceInfoManagerTest, CheckDeviceInfoNoLanguagePreference) {
  base::test::TestFuture<DeviceInfo> info_future;
  device_info_manager()->GetDeviceInfo(info_future.GetCallback());

  DeviceInfo device_info = info_future.Take();

  // If there's no preferred locale set in prefs, locale should fall back to the
  // current UI language.
  ASSERT_EQ(device_info.locale, g_browser_process->GetApplicationLocale());
}

TEST_F(DeviceInfoManagerTest, GetDeviceInfoMultipleTimes) {
  statistics_provider()->SetMachineStatistic(ash::system::kHardwareClassKey,
                                             "FOOBAR D0G-F4N-C1UB");

  base::test::TestFuture<DeviceInfo> info_future_1;
  base::test::TestFuture<DeviceInfo> info_future_2;
  base::test::TestFuture<DeviceInfo> info_future_3;

  device_info_manager()->GetDeviceInfo(info_future_1.GetCallback());
  device_info_manager()->GetDeviceInfo(info_future_2.GetCallback());

  DeviceInfo device_info_1 = info_future_1.Take();
  DeviceInfo device_info_2 = info_future_2.Take();

  device_info_manager()->GetDeviceInfo(info_future_3.GetCallback());

  DeviceInfo device_info_3 = info_future_3.Take();

  // Check that we obtained the same result for all three DeviceInfos.
  ASSERT_EQ(device_info_1.hardware_id, device_info_2.hardware_id);
  ASSERT_EQ(device_info_1.hardware_id, device_info_3.hardware_id);
}

TEST_F(DeviceInfoManagerTest, DeviceInfoToProto) {
  DeviceInfo device_info;
  device_info.board = "brya";
  device_info.model = "taniks";
  device_info.hardware_id = "FOOBAR D0G-F4N-C1UB";
  device_info.user_type = "unmanaged";
  device_info.version_info.ash_chrome = "10.10.10";
  device_info.version_info.platform = "12345.0.0";
  device_info.version_info.channel = version_info::Channel::STABLE;
  device_info.version_info.arc_sdk = 33;
  device_info.version_info.steam_client = "TRUE";
  device_info.locale = "en-US";
  device_info.custom_label_tag = "COOL-OEM";

  proto::ClientDeviceContext device_context = device_info.ToDeviceContext();

  EXPECT_EQ(device_context.board(), "brya");
  EXPECT_EQ(device_context.model(), "taniks");
  EXPECT_EQ(device_context.channel(),
            apps::proto::ClientDeviceContext::CHANNEL_STABLE);
  EXPECT_EQ(device_context.versions().chrome_ash(), "10.10.10");
  EXPECT_EQ(device_context.versions().chrome_os_platform(), "12345.0.0");
  EXPECT_EQ(device_context.versions().arc_sdk(), 33);
  EXPECT_EQ(device_context.versions().steam_client(), "TRUE");
  EXPECT_EQ(device_context.hardware_id(), "FOOBAR D0G-F4N-C1UB");
  EXPECT_EQ(device_context.custom_label_tag(), "COOL-OEM");

  proto::ClientUserContext user_context = device_info.ToUserContext();

  EXPECT_EQ(user_context.language(), "en-US");
  EXPECT_EQ(user_context.user_type(),
            apps::proto::ClientUserContext::USERTYPE_UNMANAGED);
}

TEST_F(DeviceInfoManagerTest, UserTypeToProto) {
  {
    DeviceInfo info;
    info.user_type = "unmanaged";
    EXPECT_EQ(info.ToUserContext().user_type(),
              proto::ClientUserContext::USERTYPE_UNMANAGED);
  }
  {
    DeviceInfo info;
    info.user_type = "managed";
    EXPECT_EQ(info.ToUserContext().user_type(),
              proto::ClientUserContext::USERTYPE_MANAGED);
  }
  {
    DeviceInfo info;
    info.user_type = "child";
    EXPECT_EQ(info.ToUserContext().user_type(),
              proto::ClientUserContext::USERTYPE_CHILD);
  }
  {
    DeviceInfo info;
    info.user_type = "guest";
    EXPECT_EQ(info.ToUserContext().user_type(),
              proto::ClientUserContext::USERTYPE_GUEST);
  }
  {
    DeviceInfo info;
    info.user_type = "managed_guest";
    EXPECT_EQ(info.ToUserContext().user_type(),
              proto::ClientUserContext::USERTYPE_MANAGED_GUEST);
  }
  {
    DeviceInfo info;
    info.user_type = "dog";
    EXPECT_EQ(info.ToUserContext().user_type(),
              proto::ClientUserContext::USERTYPE_UNKNOWN);
  }
}

TEST_F(DeviceInfoManagerTest, ChannelTypeToProto) {
  {
    DeviceInfo info;
    info.version_info.channel = version_info::Channel::CANARY;
    EXPECT_EQ(info.ToDeviceContext().channel(),
              proto::ClientDeviceContext::CHANNEL_CANARY);
  }
  {
    DeviceInfo info;
    info.version_info.channel = version_info::Channel::DEV;
    EXPECT_EQ(info.ToDeviceContext().channel(),
              proto::ClientDeviceContext::CHANNEL_DEV);
  }
  {
    DeviceInfo info;
    info.version_info.channel = version_info::Channel::BETA;
    EXPECT_EQ(info.ToDeviceContext().channel(),
              proto::ClientDeviceContext::CHANNEL_BETA);
  }
  {
    DeviceInfo info;
    info.version_info.channel = version_info::Channel::STABLE;
    EXPECT_EQ(info.ToDeviceContext().channel(),
              proto::ClientDeviceContext::CHANNEL_STABLE);
  }
  {
    DeviceInfo info;
    info.version_info.channel = version_info::Channel::UNKNOWN;
    EXPECT_EQ(info.ToDeviceContext().channel(),
              proto::ClientDeviceContext::CHANNEL_DEFAULT);
  }
}

}  // namespace apps
