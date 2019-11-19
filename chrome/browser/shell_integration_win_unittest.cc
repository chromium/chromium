// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shell_integration_win.h"

#include <stddef.h>

#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_shortcut_win.h"
#include "base/win/scoped_com_initializer.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_shortcut_win.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/installer/util/util_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace shell_integration {
namespace win {

namespace {

struct ShortcutTestObject {
  base::FilePath path;
  base::win::ShortcutProperties properties;
};

class ShellIntegrationWinMigrateShortcutTest : public testing::Test {
 protected:
  ShellIntegrationWinMigrateShortcutTest() {}

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(
        temp_dir_sub_dir_.CreateUniqueTempDirUnderPath(temp_dir_.GetPath()));
    // A path to a random target.
    base::CreateTemporaryFileInDir(temp_dir_.GetPath(), &other_target_);

    // This doesn't need to actually have a base name of "chrome.exe".
    base::CreateTemporaryFileInDir(temp_dir_.GetPath(), &chrome_exe_);

    chrome_app_id_ = ShellUtil::GetBrowserModelId(true);

    base::FilePath default_user_data_dir;
    chrome::GetDefaultUserDataDirectory(&default_user_data_dir);
    base::FilePath default_profile_path =
        default_user_data_dir.AppendASCII(chrome::kInitialProfile);
    non_default_user_data_dir_ = base::FilePath(FILE_PATH_LITERAL("root"))
        .Append(FILE_PATH_LITERAL("Non Default Data Dir"));
    non_default_profile_ = L"NonDefault";
    non_default_profile_chrome_app_id_ = GetChromiumModelIdForProfile(
        default_user_data_dir.Append(non_default_profile_));
    non_default_user_data_dir_chrome_app_id_ = GetChromiumModelIdForProfile(
        non_default_user_data_dir_.AppendASCII(chrome::kInitialProfile));
    non_default_user_data_dir_and_profile_chrome_app_id_ =
        GetChromiumModelIdForProfile(
            non_default_user_data_dir_.Append(non_default_profile_));
    default_profile_ = L"Default";
    default_profile_chrome_app_id_ = GetChromiumModelIdForProfile(
        default_user_data_dir.Append(default_profile_));

    extension_id_ = L"chromiumexampleappidforunittests";
    base::string16 app_name =
        base::UTF8ToUTF16(web_app::GenerateApplicationNameFromAppId(
            base::UTF16ToUTF8(extension_id_)));
    extension_app_id_ = GetAppModelIdForProfile(app_name, default_profile_path);
    non_default_profile_extension_app_id_ = GetAppModelIdForProfile(
        app_name, default_user_data_dir.Append(non_default_profile_));
  }

  // Creates a test shortcut corresponding to |shortcut_properties| and resets
  // |shortcut_properties| after copying it to an internal structure for later
  // verification.
  void AddTestShortcutAndResetProperties(
      const base::FilePath& shortcut_dir,
      base::win::ShortcutProperties* shortcut_properties) {
    ShortcutTestObject shortcut_test_object;
    base::FilePath shortcut_path = shortcut_dir.Append(
        L"Shortcut " + base::NumberToString16(shortcuts_.size()) +
        installer::kLnkExt);
    shortcut_test_object.path = shortcut_path;
    shortcut_test_object.properties = *shortcut_properties;
    shortcuts_.push_back(shortcut_test_object);
    ASSERT_TRUE(base::win::CreateOrUpdateShortcutLink(
        shortcut_path, *shortcut_properties,
        base::win::SHORTCUT_CREATE_ALWAYS));
    shortcut_properties->options = 0U;
  }

  void CreateShortcuts() {
    // A temporary object to pass properties to
    // AddTestShortcutAndResetProperties().
    base::win::ShortcutProperties temp_properties;

    // Shortcut 0 doesn't point to chrome.exe and thus should never be migrated.
    temp_properties.set_target(other_target_);
    temp_properties.set_app_id(L"Dumbo");
    ASSERT_NO_FATAL_FAILURE(AddTestShortcutAndResetProperties(
        temp_dir_.GetPath(), &temp_properties));

    // Shortcut 1 points to chrome.exe and thus should be migrated.
    temp_properties.set_target(chrome_exe_);
    temp_properties.set_app_id(L"Dumbo");
    temp_properties.set_dual_mode(false);
    ASSERT_NO_FATAL_FAILURE(AddTestShortcutAndResetProperties(
        temp_dir_.GetPath(), &temp_properties));

    // Shortcut 2 points to chrome.exe, but already has the right appid and thus
    // should only be migrated if dual_mode is desired.
    temp_properties.set_target(chrome_exe_);
    temp_properties.set_app_id(default_profile_chrome_app_id_);
    ASSERT_NO_FATAL_FAILURE(AddTestShortcutAndResetProperties(
        temp_dir_.GetPath(), &temp_properties));

    // Shortcut 3 is like shortcut 1, but it's appid is a prefix of the expected
    // appid instead of being totally different.
    base::string16 chrome_app_id_is_prefix(default_profile_chrome_app_id_);
    chrome_app_id_is_prefix.push_back(L'1');
    temp_properties.set_target(chrome_exe_);
    temp_properties.set_app_id(chrome_app_id_is_prefix);
    ASSERT_NO_FATAL_FAILURE(AddTestShortcutAndResetProperties(
        temp_dir_.GetPath(), &temp_properties));

    // Shortcut 4 is like shortcut 1, but it's appid is of the same size as the
    // expected appid.
    base::string16 same_size_as_chrome_app_id(
        default_profile_chrome_app_id_.size(), L'1');
    temp_properties.set_target(chrome_exe_);
    temp_properties.set_app_id(same_size_as_chrome_app_id);
    ASSERT_NO_FATAL_FAILURE(AddTestShortcutAndResetProperties(
        temp_dir_.GetPath(), &temp_properties));

    // Shortcut 5 doesn't have an app_id, nor is dual_mode even set; they should
    // be set as expected upon migration.
    temp_properties.set_target(chrome_exe_);
    ASSERT_NO_FATAL_FAILURE(AddTestShortcutAndResetProperties(
        temp_dir_.GetPath(), &temp_properties));

    // Shortcut 6 has a non-default profile directory and so should get a non-
    // default app id.
    temp_properties.set_target(chrome_exe_);
    temp_properties.set_app_id(L"Dumbo");
    temp_properties.set_arguments(
        L"--profile-directory=" + non_default_profile_);
    ASSERT_NO_FATAL_FAILURE(AddTestShortcutAndResetProperties(
        temp_dir_.GetPath(), &temp_properties));

    // Shortcut 7 has a non-default user data directory and so should get a non-
    // default app id.
    temp_properties.set_target(chrome_exe_);
    temp_properties.set_app_id(L"Dumbo");
    temp_properties.set_arguments(
        L"--user-data-dir=\"" + non_default_user_data_dir_.value() + L"\"");
    ASSERT_NO_FATAL_FAILURE(AddTestShortcutAndResetProperties(
        temp_dir_.GetPath(), &temp_properties));

    // Shortcut 8 has a non-default user data directory as well as a non-default
    // profile directory and so should get a non-default app id.
    temp_properties.set_target(chrome_exe_);
    temp_properties.set_app_id(L"Dumbo");
    temp_properties.set_arguments(
        L"--user-data-dir=\"" + non_default_user_data_dir_.value() + L"\" " +
        L"--profile-directory=" + non_default_profile_);
    ASSERT_NO_FATAL_FAILURE(AddTestShortcutAndResetProperties(
        temp_dir_.GetPath(), &temp_properties));

    // Shortcut 9 is a shortcut to an app and should get an app id for that app
    // rather than the chrome app id.
    temp_properties.set_target(chrome_exe_);
    temp_properties.set_app_id(L"Dumbo");
    temp_properties.set_arguments(
        L"--app-id=" + extension_id_);
    ASSERT_NO_FATAL_FAILURE(AddTestShortcutAndResetProperties(
        temp_dir_.GetPath(), &temp_properties));

    // Shortcut 10 is a shortcut to an app with a non-default profile and should
    // get an app id for that app with a non-default app id rather than the
    // chrome app id.
    temp_properties.set_target(chrome_exe_);
    temp_properties.set_app_id(L"Dumbo");
    temp_properties.set_arguments(
        L"--app-id=" + extension_id_ +
        L" --profile-directory=" + non_default_profile_);
    ASSERT_NO_FATAL_FAILURE(AddTestShortcutAndResetProperties(
        temp_dir_.GetPath(), &temp_properties));

    // Shortcut 11 points to chrome.exe, has the chrome appid, and has
    // dual_mode set and thus should be migrated to the
    // default_profile_chrome_app_id, and dual_mode should be cleared.
    temp_properties.set_target(chrome_exe_);
    temp_properties.set_app_id(chrome_app_id_);
    temp_properties.set_dual_mode(true);
    ASSERT_NO_FATAL_FAILURE(AddTestShortcutAndResetProperties(
        temp_dir_.GetPath(), &temp_properties));

    // Shortcut 12 is similar to 11 but with dual_mode explicitly set to false.
    temp_properties.set_target(chrome_exe_);
    temp_properties.set_app_id(chrome_app_id_);
    temp_properties.set_dual_mode(false);
    ASSERT_NO_FATAL_FAILURE(AddTestShortcutAndResetProperties(
        temp_dir_.GetPath(), &temp_properties));
  }

  base::win::ScopedCOMInitializer com_initializer_;

  base::ScopedTempDir temp_dir_;

  // Used to test migration of shortcuts in ImplicitApps sub-directories.
  base::ScopedTempDir temp_dir_sub_dir_;

  // Test shortcuts.
  std::vector<ShortcutTestObject> shortcuts_;

  // The path to a fake chrome.exe.
  base::FilePath chrome_exe_;

  // The path to a random target.
  base::FilePath other_target_;

  // Chrome's AppUserModelId.
  base::string16 chrome_app_id_;

  // Chrome's AppUserModelId for the default profile.
  base::string16 default_profile_chrome_app_id_;

  // The Default profile.
  base::string16 default_profile_;

  // A profile that isn't the Default profile.
  base::string16 non_default_profile_;

  // A user data dir that isn't the default.
  base::FilePath non_default_user_data_dir_;

  // Chrome's AppUserModelId for the non-default profile.
  base::string16 non_default_profile_chrome_app_id_;

  // Chrome's AppUserModelId for the non-default user data dir.
  base::string16 non_default_user_data_dir_chrome_app_id_;

  // Chrome's AppUserModelId for the non-default user data dir and non-default
  // profile.
  base::string16 non_default_user_data_dir_and_profile_chrome_app_id_;

  // An example extension id of an example app.
  base::string16 extension_id_;

  // The app id of the example app for the default profile and user data dir.
  base::string16 extension_app_id_;

  // The app id of the example app for the non-default profile.
  base::string16 non_default_profile_extension_app_id_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShellIntegrationWinMigrateShortcutTest);
};

}  // namespace

TEST_F(ShellIntegrationWinMigrateShortcutTest, ClearDualModeAndAdjustAppIds) {
  CreateShortcuts();
  // 11 shortcuts should have their app id updated below including shortcut 11,
  // which should also be migrated away from dual_mode.
  EXPECT_EQ(11,
            MigrateShortcutsInPathInternal(chrome_exe_, temp_dir_.GetPath()));

  // Shortcut 1, 3, 4, 5, 6, 7, 8, 9, 10, 11 and 12 should have had their app_id
  // fixed.
  shortcuts_[1].properties.set_app_id(default_profile_chrome_app_id_);
  shortcuts_[3].properties.set_app_id(default_profile_chrome_app_id_);
  shortcuts_[4].properties.set_app_id(default_profile_chrome_app_id_);
  shortcuts_[5].properties.set_app_id(default_profile_chrome_app_id_);
  shortcuts_[6].properties.set_app_id(non_default_profile_chrome_app_id_);
  shortcuts_[7].properties.set_app_id(non_default_user_data_dir_chrome_app_id_);
  shortcuts_[8].properties.set_app_id(
      non_default_user_data_dir_and_profile_chrome_app_id_);
  shortcuts_[9].properties.set_app_id(extension_app_id_);
  shortcuts_[10].properties.set_app_id(non_default_profile_extension_app_id_);
  shortcuts_[11].properties.set_app_id(default_profile_chrome_app_id_);
  shortcuts_[12].properties.set_app_id(default_profile_chrome_app_id_);

  // No shortcut should still have the dual_mode property.
  for (size_t i = 0; i < shortcuts_.size(); ++i)
    shortcuts_[i].properties.set_dual_mode(false);

  for (size_t i = 0; i < shortcuts_.size(); ++i) {
    SCOPED_TRACE(i);
    base::win::ValidateShortcut(shortcuts_[i].path, shortcuts_[i].properties);
  }

  // Make sure shortcuts are not re-migrated.
  EXPECT_EQ(0,
            MigrateShortcutsInPathInternal(chrome_exe_, temp_dir_.GetPath()));
}

// Test that chrome_proxy.exe shortcuts (PWA) have their app_id migrated
// to include the default profile name. This tests both shortcuts in the
// DIR_TASKBAR_PINS and sub-directories of DIR_IMPLICIT_APP_SHORTCUTS.
TEST_F(ShellIntegrationWinMigrateShortcutTest, MigrateChromeProxyTest) {
  // Create shortcut to chrome_proxy_exe in executable directory,
  // using the default profile, with the AppModelId not containing the
  // profile name. "chrome_proxy.exe" won't exist, but it appears that
  // creating the shortcut succeeds.
  base::win::ShortcutProperties temp_properties;
  temp_properties.set_target(web_app::GetChromeProxyPath());
  temp_properties.set_app_id(L"Dumbo");
  ASSERT_NO_FATAL_FAILURE(
      AddTestShortcutAndResetProperties(temp_dir_.GetPath(), &temp_properties));
  temp_properties.set_target(web_app::GetChromeProxyPath());
  temp_properties.set_app_id(L"Dumbo2");
  ASSERT_NO_FATAL_FAILURE(AddTestShortcutAndResetProperties(
      temp_dir_sub_dir_.GetPath(), &temp_properties));

  MigrateTaskbarPinsCallback(temp_dir_.GetPath(), temp_dir_.GetPath());
  // Verify that the migrated shortcut in temp_dir_ now contains the default
  // profile name.
  shortcuts_[0].properties.set_app_id(default_profile_chrome_app_id_);
  base::win::ValidateShortcut(shortcuts_[0].path, shortcuts_[0].properties);
  // Verify that the migrated shortcut in temp_dir_sub now contains the default
  // profile name.
  shortcuts_[1].properties.set_app_id(default_profile_chrome_app_id_);
  base::win::ValidateShortcut(shortcuts_[1].path, shortcuts_[1].properties);
}

TEST(ShellIntegrationWinTest, GetAppModelIdForProfileTest) {
  const base::string16 base_app_id(install_static::GetBaseAppId());

  // Empty profile path should get chrome::kBrowserAppID
  base::FilePath empty_path;
  EXPECT_EQ(base_app_id, GetAppModelIdForProfile(base_app_id, empty_path));

  // Default profile path should get chrome::kBrowserAppID joined with
  // profile info.
  base::FilePath default_user_data_dir;
  chrome::GetDefaultUserDataDirectory(&default_user_data_dir);
  base::FilePath default_profile_path =
      default_user_data_dir.AppendASCII(chrome::kInitialProfile);
  EXPECT_EQ(base_app_id + L".UserData.Default",
            GetAppModelIdForProfile(base_app_id, default_profile_path));

  // Non-default profile path should get chrome::kBrowserAppID joined with
  // profile info.
  base::FilePath profile_path(FILE_PATH_LITERAL("root"));
  profile_path = profile_path.Append(FILE_PATH_LITERAL("udd"));
  profile_path = profile_path.Append(FILE_PATH_LITERAL("User Data - Test"));
  EXPECT_EQ(base_app_id + L".udd.UserDataTest",
            GetAppModelIdForProfile(base_app_id, profile_path));
}

}  // namespace win
}  // namespace shell_integration
