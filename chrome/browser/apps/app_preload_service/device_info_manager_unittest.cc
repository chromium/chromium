// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_preload_service/device_info_manager.h"

#include <memory>

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "chrome/common/channel_info.h"
#include "chrome/test/base/testing_profile.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

static constexpr char kTestLocale[] = "test_locale";

}  // namespace

namespace apps {

class DeviceInfoManagerTest : public testing::Test {
 public:
  std::unique_ptr<DeviceInfoManager> device_info_manager_;

  void VerifyDeviceInfo(base::OnceClosure on_complete, DeviceInfo device_info) {
    ASSERT_FALSE(device_info.board.empty());
    ASSERT_FALSE(device_info.model.empty());
    ASSERT_FALSE(device_info.user_type.empty());
    ASSERT_FALSE(device_info.version_info.ash_chrome.empty());
    ASSERT_FALSE(device_info.version_info.platform.empty());
    ASSERT_EQ(device_info.version_info.channel, chrome::GetChannel());
    ASSERT_EQ(device_info.locale, kTestLocale);
    std::move(on_complete).Run();
  }

 protected:
  DeviceInfoManagerTest() {
    device_info_manager_ = std::make_unique<DeviceInfoManager>(&profile_);
    PrefService* prefs = profile_.GetPrefs();
    prefs->SetString(language::prefs::kApplicationLocale, kTestLocale);
  }

 private:
  // To support context of browser threads.
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(DeviceInfoManagerTest, CheckDeviceInfo) {
  ASSERT_TRUE(device_info_manager_ != nullptr);

  base::RunLoop run_loop;

  device_info_manager_->GetDeviceInfo(
      base::BindOnce(&DeviceInfoManagerTest::VerifyDeviceInfo,
                     base::Unretained(this), run_loop.QuitClosure()));

  run_loop.Run();
}

}  // namespace apps
