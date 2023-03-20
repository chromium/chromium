// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/almanac_api_client/device_info_manager.h"

#include <memory>

#include "base/test/scoped_chromeos_version_info.h"
#include "base/test/test_future.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/channel_info.h"
#include "chrome/test/base/testing_profile.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

class DeviceInfoManagerTest : public testing::Test {
 public:
  void SetUp() override {
    device_info_manager_ = std::make_unique<DeviceInfoManager>(&profile_);
  }

  Profile* profile() { return &profile_; }
  DeviceInfoManager* device_info_manager() {
    return device_info_manager_.get();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;

  std::unique_ptr<DeviceInfoManager> device_info_manager_;
};

TEST_F(DeviceInfoManagerTest, CheckDeviceInfo) {
  const char kLsbRelease[] = R"(
  CHROMEOS_RELEASE_VERSION=123.4.5
  CHROMEOS_RELEASE_BOARD=puff-signed-mp-v11keys
  )";
  base::test::ScopedChromeOSVersionInfo version(kLsbRelease, base::Time());

  static constexpr char kTestLocale[] = "test_locale";
  profile()->GetPrefs()->SetString(language::prefs::kApplicationLocale,
                                   kTestLocale);

  base::test::TestFuture<DeviceInfo> info_future;
  device_info_manager()->GetDeviceInfo(info_future.GetCallback());

  DeviceInfo device_info = info_future.Take();

  // Values set above:
  ASSERT_EQ(device_info.board, "puff");
  ASSERT_FALSE(device_info.model.empty());
  ASSERT_EQ(device_info.user_type, "unmanaged");
  ASSERT_FALSE(device_info.version_info.ash_chrome.empty());
  ASSERT_EQ(device_info.version_info.platform, "123.4.5");
  ASSERT_EQ(device_info.version_info.channel, chrome::GetChannel());
  ASSERT_EQ(device_info.locale, kTestLocale);
}

TEST_F(DeviceInfoManagerTest, CheckDeviceInfoNoLanguagePreference) {
  base::test::TestFuture<DeviceInfo> info_future;
  device_info_manager()->GetDeviceInfo(info_future.GetCallback());

  DeviceInfo device_info = info_future.Take();

  // If there's no preferred locale set in prefs, locale should fall back to the
  // current UI language.
  ASSERT_EQ(device_info.locale, g_browser_process->GetApplicationLocale());
}

}  // namespace apps
