// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/nearby_sharing/nearby_share_default_device_name.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"

namespace {

const char kFakeGivenName[] = "Josh";
const char kFakeEmail[] = "fake_account_id@gmail.com";

}  // namespace

TEST(NearbyShareDefaultDeviceNameTest, DefaultDeviceName) {
  content::BrowserTaskEnvironment task_environment;

  // Configure test profile.
  TestingProfileManager profile_manager(TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(profile_manager.SetUp());
  user_manager::FakeUserManager* fake_user_manager =
      new user_manager::FakeUserManager();
  user_manager::ScopedUserManager enabler(base::WrapUnique(fake_user_manager));
  Profile* profile = profile_manager.CreateTestingProfile(kFakeEmail);
  AccountId id = AccountId::FromUserEmail(kFakeEmail);
  fake_user_manager->AddUser(id);

  // If given name is empty, only return the device type.
  fake_user_manager->UpdateUserAccountData(
      id, user_manager::UserManager::UserAccountData(
              /*display_name=*/base::string16(),
              /*given_name=*/base::string16(),
              /*locale=*/std::string()));
  EXPECT_EQ(base::UTF16ToUTF8(ui::GetChromeOSDeviceName()),
            GetNearbyShareDefaultDeviceName(profile));

  // Set given name and expect full default device name of the form
  // "<given name>'s <device type>."
  fake_user_manager->UpdateUserAccountData(
      id, user_manager::UserManager::UserAccountData(
              /*display_name=*/base::string16(),
              /*given_name=*/base::UTF8ToUTF16(kFakeGivenName),
              /*locale=*/std::string()));
  EXPECT_EQ(l10n_util::GetStringFUTF8(IDS_NEARBY_DEFAULT_DEVICE_NAME,
                                      base::UTF8ToUTF16(kFakeGivenName),
                                      ui::GetChromeOSDeviceName()),
            GetNearbyShareDefaultDeviceName(profile));
}
