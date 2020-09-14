// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/system/sys_info.h"
#include "base/test/bind_test_util.h"
#include "build/build_config.h"
#include "chrome/browser/nearby_sharing/nearby_share_default_device_name.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#if defined(OS_CHROMEOS)
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "ui/chromeos/devicetype_utils.h"
#endif  // defined(OS_CHROMEOS)

namespace {

const char kFakeNameFromProfile[] = "Profile Name";
#if defined(OS_CHROMEOS)
const char kFakeEmail[] = "fake_account_id@gmail.com";
#endif  // defined(OS_CHROMEOS)

std::string GetModelNameBlocking() {
  base::RunLoop run_loop;
  std::string model_name;
  base::SysInfo::GetHardwareInfo(base::BindLambdaForTesting(
      [&](base::SysInfo::HardwareInfo hardware_info) {
        model_name = std::move(hardware_info.model);
        run_loop.Quit();
      }));
  run_loop.Run();
  return model_name;
}

}  // namespace

TEST(NearbyShareDefaultDeviceNameTest, DefaultDeviceName) {
  content::BrowserTaskEnvironment task_environment;

  // Configure test profile.
  TestingProfileManager profile_manager(TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(profile_manager.SetUp());
  Profile* profile = nullptr;
#if defined(OS_CHROMEOS)
  chromeos::FakeChromeUserManager* user_manager =
      new chromeos::FakeChromeUserManager();
  user_manager::ScopedUserManager enabler(base::WrapUnique(user_manager));
  profile = profile_manager.CreateTestingProfile(kFakeEmail);
  user_manager->AddUser(AccountId::FromUserEmail(kFakeEmail));
  user_manager->SaveUserDisplayName(AccountId::FromUserEmail(kFakeEmail),
                                    base::UTF8ToUTF16(kFakeNameFromProfile));
#else   // !defined(OS_CHROMEOS)
  profile = profile_manager.CreateTestingProfile(kFakeNameFromProfile);
#endif  // defined(OS_CHROMEOS)

  base::Optional<std::string> device_name;
  base::RunLoop run_loop;
  GetNearbyShareDefaultDeviceName(
      profile,
      base::BindLambdaForTesting(
          [&run_loop, &device_name](const base::Optional<std::string>& name) {
            device_name = std::move(name);
            run_loop.Quit();
          }));
  run_loop.Run();

#if defined(OS_CHROMEOS)
  EXPECT_EQ(std::string(kFakeNameFromProfile) + "'s " +
                base::UTF16ToUTF8(ui::GetChromeOSDeviceName()),
            *device_name);
#else   // !defined(OS_CHROMEOS)
  std::string expected_model_name = GetModelNameBlocking();
  if (expected_model_name.empty()) {
    EXPECT_TRUE(
        device_name->rfind(std::string(kFakeNameFromProfile) + "'s ", 0) == 0);
  } else {
    EXPECT_EQ(std::string(kFakeNameFromProfile) + "'s " + expected_model_name,
              device_name);
  }
#endif  // defined(OS_CHROMEOS)
}
