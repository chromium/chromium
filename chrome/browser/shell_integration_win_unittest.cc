// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shell_integration_win.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "base/base_paths.h"
#include "base/base_paths_win.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_path_override.h"
#include "base/test/test_shortcut_win.h"
#include "base/win/scoped_com_initializer.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/shortcuts/platform_util_win.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/install_static/install_details.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/installer/util/util_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace shell_integration {
namespace win {

namespace {

struct ShortcutTestObject {
  base::FilePath path;
  base::win::ShortcutProperties properties;
};

class ShellIntegrationWinMigrateShortcutTest : public testing::Test {
 public:
  ShellIntegrationWinMigrateShortcutTest(
      const ShellIntegrationWinMigrateShortcutTest&) = delete;
  ShellIntegrationWinMigrateShortcutTest& operator=(
      const ShellIntegrationWinMigrateShortcutTest&) = delete;

 protected:
  ShellIntegrationWinMigrateShortcutTest() = default;

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
    non_default_profile_chrome_app_id_ = GetAppUserModelIdForBrowser(
        default_user_data_dir.Append(non_default_profile_));
    non_default_user_data_dir_chrome_app_id_ = GetAppUserModelIdForBrowser(
        non_default_user_data_dir_.AppendASCII(chrome::kInitialProfile));
    non_default_user_data_dir_and_profile_chrome_app_id_ =
        GetAppUserModelIdForBrowser(
            non_default_user_data_dir_.Append(non_default_profile_));

    extension_id_ = L"chromiumexampleappidforunittests";
    std::wstring app_name =
        base::UTF8ToWide(web_app::GenerateApplicationNameFromAppId(
            base::WideToUTF8(extension_id_)));
    extension_app_id_ = GetAppUserModelIdForApp(app_name, default_profile_path);
    non_default_profile_extension_app_id_ = GetAppUserModelIdForApp(
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
        L"Shortcut " + base::NumberToWString(shortcuts_.size()) +
        installer::kLnkExt);
    shortcut_test_object.path = shortcut_path;
    shortcut_test_object.properties = *shortcut_properties;
    shortcuts_.push_back(shortcut_test_object);
    ASSERT_TRUE(base::win::CreateOrUpdateShortcutLink(
        shortcut_path, *shortcut_properties,
        base::win::ShortcutOperation::kCreateAlways));
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
    ASSERT_NO_FATAL_FAILURE(AddTestShortcutAndResetProperties(
        temp_dir_.GetPath(), &temp_properties));

    // Shortcut 2 points to chrome.exe, and already has the right appid and
    // source shortcut location.
    temp_properties.set_target(chrome_exe_);
    temp_properties.set_app_id(chrome_app_id_);
    temp_properties.set_arguments(L"--source-shortcut-location=taskbar");
    ASSERT_NO_FATAL_FAILURE(AddTestShortcutAndResetProperties(
        temp_dir_.GetPath(), &temp_properties));

    // Shortcut 3 is like shortcut 1, but it's appid is a prefix of the expected
    // appid instead of being totally different.
    std::wstring chrome_app_id_is_prefix(chrome_app_id_);
    chrome_app_id_is_prefix.push_back(L'1');
    temp_properties.set_target(chrome_exe_);
    temp_properties.set_app_id(chrome_app_id_is_prefix);
    ASSERT_NO_FATAL_FAILURE(AddTestShortcutAndResetProperties(
        temp_dir_.GetPath(), &temp_properties));

    // Shortcut 4 is like shortcut 1, but it's appid is of the same size as the
    // expected appid.
    std::wstring same_size_as_chrome_app_id(chrome_app_id_.size(), L'1');
    temp_properties.set_target(chrome_exe_);
    temp_properties.set_app_id(same_size_as_chrome_app_id);
    ASSERT_NO_FATAL_FAILURE(AddTestShortcutAndResetProperties(
        temp_dir_.GetPath(), &temp_properties));

    // Shortcut 5 doesn't have an app_id, it should
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

    // Shortcut 11 is like shortcut 1, but it's appid explicitly includes the
    // default profile.
    std::wstring chrome_app_id_with_default_profile =
        chrome_app_id_ + L".Default";
    temp_properties.set_target(chrome_exe_);
    temp_properties.set_app_id(chrome_app_id_with_default_profile);
    ASSERT_NO_FATAL_FAILURE(AddTestShortcutAndResetProperties(
        temp_dir_.GetPath(), &temp_properties));

    // Shortcut 12 already has the right appid, but is missing
    // --source-shortcut-location=taskbar.
    temp_properties.set_target(chrome_exe_);
    temp_properties.set_app_id(chrome_app_id_);
    ASSERT_NO_FATAL_FAILURE(AddTestShortcutAndResetProperties(
        temp_dir_.GetPath(), &temp_properties));

    // Shortcut 13 already has the right appid, but was pinned from the Start
    // Menu and carries --source-shortcut-location=start-menu.
    temp_properties.set_target(chrome_exe_);
    temp_properties.set_app_id(chrome_app_id_);
    temp_properties.set_arguments(L"--source-shortcut-location=start-menu");
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
  std::wstring chrome_app_id_;

  // A profile that isn't the Default profile.
  std::wstring non_default_profile_;

  // A user data dir that isn't the default.
  base::FilePath non_default_user_data_dir_;

  // Chrome's AppUserModelId for the non-default profile.
  std::wstring non_default_profile_chrome_app_id_;

  // Chrome's AppUserModelId for the non-default user data dir.
  std::wstring non_default_user_data_dir_chrome_app_id_;

  // Chrome's AppUserModelId for the non-default user data dir and non-default
  // profile.
  std::wstring non_default_user_data_dir_and_profile_chrome_app_id_;

  // An example extension id of an example app.
  std::wstring extension_id_;

  // The app id of the example app for the default profile and user data dir.
  std::wstring extension_app_id_;

  // The app id of the example app for the non-default profile.
  std::wstring non_default_profile_extension_app_id_;
};

}  // namespace

TEST_F(ShellIntegrationWinMigrateShortcutTest, AdjustAppIds) {
  CreateShortcuts();
  // 12 shortcuts should have their app id or arguments updated below.
  EXPECT_EQ(12,
            MigrateShortcutsInPathInternal(chrome_exe_, temp_dir_.GetPath()));

  // Shortcut 1, 3, 4, 5, 6, 7, 8, 9, 10, and 11 should have had their app_id
  // fixed, and shortcuts 1 and 3..13 should have
  // --source-shortcut-location=taskbar.
  shortcuts_[1].properties.set_app_id(chrome_app_id_);
  shortcuts_[1].properties.set_arguments(L"--source-shortcut-location=taskbar");
  shortcuts_[3].properties.set_app_id(chrome_app_id_);
  shortcuts_[3].properties.set_arguments(L"--source-shortcut-location=taskbar");
  shortcuts_[4].properties.set_app_id(chrome_app_id_);
  shortcuts_[4].properties.set_arguments(L"--source-shortcut-location=taskbar");
  shortcuts_[5].properties.set_app_id(chrome_app_id_);
  shortcuts_[5].properties.set_arguments(L"--source-shortcut-location=taskbar");
  shortcuts_[6].properties.set_app_id(non_default_profile_chrome_app_id_);
  shortcuts_[6].properties.set_arguments(
      base::StrCat({shortcuts_[6].properties.arguments,
                    L" --source-shortcut-location=taskbar"}));
  shortcuts_[7].properties.set_app_id(non_default_user_data_dir_chrome_app_id_);
  shortcuts_[7].properties.set_arguments(
      base::StrCat({shortcuts_[7].properties.arguments,
                    L" --source-shortcut-location=taskbar"}));
  shortcuts_[8].properties.set_app_id(
      non_default_user_data_dir_and_profile_chrome_app_id_);
  shortcuts_[8].properties.set_arguments(
      base::StrCat({shortcuts_[8].properties.arguments,
                    L" --source-shortcut-location=taskbar"}));
  shortcuts_[9].properties.set_app_id(extension_app_id_);
  shortcuts_[9].properties.set_arguments(
      base::StrCat({shortcuts_[9].properties.arguments,
                    L" --source-shortcut-location=taskbar"}));
  shortcuts_[10].properties.set_app_id(non_default_profile_extension_app_id_);
  shortcuts_[10].properties.set_arguments(
      base::StrCat({shortcuts_[10].properties.arguments,
                    L" --source-shortcut-location=taskbar"}));
  shortcuts_[11].properties.set_app_id(chrome_app_id_);
  shortcuts_[11].properties.set_arguments(
      L"--source-shortcut-location=taskbar");
  shortcuts_[12].properties.set_arguments(
      L"--source-shortcut-location=taskbar");
  shortcuts_[13].properties.set_arguments(
      L"--source-shortcut-location=taskbar");

  for (size_t i = 0; i < shortcuts_.size(); ++i) {
    SCOPED_TRACE(i);
    base::win::ValidateShortcut(shortcuts_[i].path, shortcuts_[i].properties);
  }

  // Make sure shortcuts are not re-migrated.
  EXPECT_EQ(0,
            MigrateShortcutsInPathInternal(chrome_exe_, temp_dir_.GetPath()));
}

// Test that chrome_proxy.exe shortcuts (PWA) have their app_id migrated to not
// include the default profile name, are tagged with
// --source-shortcut-location=taskbar, and record
// Windows.TaskbarShortcutMigrationCount. This tests both shortcuts in
// DIR_TASKBAR_PINS and sub-directories of DIR_IMPLICIT_APP_SHORTCUTS.
TEST_F(ShellIntegrationWinMigrateShortcutTest, MigrateChromeProxyTest) {
  // Create shortcut to chrome_proxy_exe in executable directory,
  // using the default profile, with the AppModelId not containing the
  // profile name.
  base::win::ShortcutProperties temp_properties;
  temp_properties.set_target(shortcuts::GetChromeProxyPath());
  temp_properties.set_app_id(L"Dumbo.Default");
  ASSERT_NO_FATAL_FAILURE(
      AddTestShortcutAndResetProperties(temp_dir_.GetPath(), &temp_properties));
  temp_properties.set_target(shortcuts::GetChromeProxyPath());
  temp_properties.set_app_id(L"Dumbo2.Default");
  ASSERT_NO_FATAL_FAILURE(AddTestShortcutAndResetProperties(
      temp_dir_sub_dir_.GetPath(), &temp_properties));

  // Check that a chrome proxy shortcut whose app_id is just the extension app
  // id has its AUMI migrated to start with the browser's app_id.
  // It technically doesn't matter what ShortcutProperties's app_id is,
  // since the migration is based on ShortcutProperties.arguments.
  temp_properties.set_target(shortcuts::GetChromeProxyPath());
  temp_properties.set_app_id(L"Dumbo3.Default");
  base::CommandLine cmd_line = shell_integration::CommandLineArgsForLauncher(
      GURL(), base::WideToUTF8(extension_id_), base::FilePath(), "");
  ASSERT_EQ(cmd_line.GetCommandLineString(), L" --app-id=" + extension_id_);
  temp_properties.set_arguments(cmd_line.GetCommandLineString());
  ASSERT_NO_FATAL_FAILURE(
      AddTestShortcutAndResetProperties(temp_dir_.GetPath(), &temp_properties));

  // Check that a chrome proxy shortcut with a kApp url in its command line
  // has its AUMI migrated to start with the browser's app_id.
  temp_properties.set_target(shortcuts::GetChromeProxyPath());
  temp_properties.set_app_id(L"Dumbo4.Default");
  GURL url("http://www.example.com");
  cmd_line = shell_integration::CommandLineArgsForLauncher(
      url, std::string(), base::FilePath(), "");
  ASSERT_EQ(cmd_line.GetCommandLineString(), L" --app=http://www.example.com/");
  temp_properties.set_arguments(cmd_line.GetCommandLineString());
  ASSERT_NO_FATAL_FAILURE(
      AddTestShortcutAndResetProperties(temp_dir_.GetPath(), &temp_properties));

  base::HistogramTester histogram_tester;
  MigrateTaskbarPinsCallback(temp_dir_.GetPath(), temp_dir_.GetPath());
  histogram_tester.ExpectUniqueSample("Windows.TaskbarShortcutMigrationCount",
                                      4, 1);

  // Verify that the migrated shortcut in temp_dir_ does not contain the default
  // profile name.
  shortcuts_[0].properties.set_app_id(chrome_app_id_);
  shortcuts_[0].properties.set_arguments(L"--source-shortcut-location=taskbar");
  base::win::ValidateShortcut(shortcuts_[0].path, shortcuts_[0].properties);
  // Verify that the migrated shortcut in temp_dir_sub does not contain the
  // default profile name.
  shortcuts_[1].properties.set_app_id(chrome_app_id_);
  shortcuts_[1].properties.set_arguments(L"--source-shortcut-location=taskbar");
  base::win::ValidateShortcut(shortcuts_[1].path, shortcuts_[1].properties);

  shortcuts_[2].properties.set_app_id(extension_app_id_);
  shortcuts_[2].properties.set_arguments(base::StrCat(
      {L"--app-id=", extension_id_, L" --source-shortcut-location=taskbar"}));
  base::win::ValidateShortcut(shortcuts_[2].path, shortcuts_[2].properties);

  shortcuts_[3].properties.set_app_id(GetAppUserModelIdForApp(
      base::UTF8ToWide(web_app::GenerateApplicationNameFromURL(url)),
      base::FilePath()));
  shortcuts_[3].properties.set_arguments(
      L"--app=http://www.example.com/ --source-shortcut-location=taskbar");
  base::win::ValidateShortcut(shortcuts_[3].path, shortcuts_[3].properties);

  // A subsequent scan when all shortcuts are already migrated should record 0.
  MigrateTaskbarPinsCallback(temp_dir_.GetPath(), temp_dir_.GetPath());
  histogram_tester.ExpectBucketCount("Windows.TaskbarShortcutMigrationCount", 0,
                                     1);
}

TEST_F(ShellIntegrationWinMigrateShortcutTest,
       MigrateShortcutLocationFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kMigrateTaskbarShortcutLocation);

  base::win::ShortcutProperties temp_properties;
  temp_properties.set_target(chrome_exe_);
  temp_properties.set_app_id(L"Dumbo");
  ASSERT_NO_FATAL_FAILURE(
      AddTestShortcutAndResetProperties(temp_dir_.GetPath(), &temp_properties));

  temp_properties.set_target(chrome_exe_);
  temp_properties.set_app_id(chrome_app_id_);
  ASSERT_NO_FATAL_FAILURE(
      AddTestShortcutAndResetProperties(temp_dir_.GetPath(), &temp_properties));

  EXPECT_EQ(1,
            MigrateShortcutsInPathInternal(chrome_exe_, temp_dir_.GetPath()));

  shortcuts_[0].properties.set_app_id(chrome_app_id_);
  base::win::ValidateShortcut(shortcuts_[0].path, shortcuts_[0].properties);
  base::win::ValidateShortcut(shortcuts_[1].path, shortcuts_[1].properties);
}

// This test verifies that MigrateTaskbarPins does a case-insensitive
// comparison when comparing the shortcut target with the chrome exe path.
TEST_F(ShellIntegrationWinMigrateShortcutTest, MigrateMixedCaseDirTest) {
  base::win::ShortcutProperties temp_properties;
  base::FilePath chrome_proxy_path(shortcuts::GetChromeProxyPath());
  ASSERT_EQ(chrome_proxy_path.Extension(), FILE_PATH_LITERAL(".exe"));
  temp_properties.set_target(
      chrome_proxy_path.ReplaceExtension(FILE_PATH_LITERAL("EXE")));
  temp_properties.set_app_id(L"Dumbo.Default");
  ASSERT_NO_FATAL_FAILURE(
      AddTestShortcutAndResetProperties(temp_dir_.GetPath(), &temp_properties));
  MigrateTaskbarPinsCallback(temp_dir_.GetPath(), temp_dir_.GetPath());
  // Verify that the shortcut was migrated, i.e., its app_id does not contain
  // the default profile name.
  shortcuts_[0].properties.set_app_id(chrome_app_id_);
  shortcuts_[0].properties.set_arguments(L"--source-shortcut-location=taskbar");
  base::win::ValidateShortcut(shortcuts_[0].path, shortcuts_[0].properties);
}

TEST_F(ShellIntegrationWinMigrateShortcutTest,
       MigrateStartMenuProfileShortcutsTest) {
  base::ScopedPathOverride exe_override(base::FILE_EXE, chrome_exe_,
                                        /*is_absolute=*/true, /*create=*/false);
  const std::wstring product_name =
      base::AsWString(l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME));

  // 1. Canonical profile shortcut pinned from Desktop to Start Menu.
  const base::FilePath profile_shortcut_path = temp_dir_.GetPath().Append(
      base::StrCat({L"Person 1 - ", product_name, installer::kLnkExt}));
  base::win::ShortcutProperties profile_props;
  profile_props.set_target(chrome_exe_);
  profile_props.set_app_id(non_default_profile_chrome_app_id_);
  profile_props.set_arguments(
      base::StrCat({L"--profile-directory=", non_default_profile_,
                    L" --source-shortcut-location=desktop"}));
  ASSERT_TRUE(base::win::CreateOrUpdateShortcutLink(
      profile_shortcut_path, profile_props,
      base::win::ShortcutOperation::kCreateAlways));

  // 2. Uniquified canonical profile shortcut ("Person 1 - Chrome (1).lnk").
  const base::FilePath uniquified_profile_shortcut_path =
      temp_dir_.GetPath().Append(base::StrCat(
          {L"Person 1 - ", product_name, L" (1)", installer::kLnkExt}));
  ASSERT_TRUE(base::win::CreateOrUpdateShortcutLink(
      uniquified_profile_shortcut_path, profile_props,
      base::win::ShortcutOperation::kCreateAlways));

  // 3. Non-profile shortcut in Start Menu (e.g., main Chrome shortcut or
  // third-party shortcut) should not be modified.
  const base::FilePath main_shortcut_path =
      temp_dir_.GetPath().Append(L"Google Chrome.lnk");
  base::win::ShortcutProperties main_props;
  main_props.set_target(chrome_exe_);
  main_props.set_app_id(chrome_app_id_);
  main_props.set_arguments(L"--source-shortcut-location=desktop");
  ASSERT_TRUE(base::win::CreateOrUpdateShortcutLink(
      main_shortcut_path, main_props,
      base::win::ShortcutOperation::kCreateAlways));

  // 4. Shortcut matching the profile filename pattern but missing
  // --profile-directory should not be modified.
  const base::FilePath fake_profile_shortcut_path = temp_dir_.GetPath().Append(
      base::StrCat({L"Fake - ", product_name, installer::kLnkExt}));
  ASSERT_TRUE(base::win::CreateOrUpdateShortcutLink(
      fake_profile_shortcut_path, main_props,
      base::win::ShortcutOperation::kCreateAlways));

  base::HistogramTester histogram_tester;
  MigrateTaskbarPinsCallback(/*pins_path=*/base::FilePath(),
                             /*implicit_apps_path=*/base::FilePath(),
                             /*start_menu_path=*/temp_dir_.GetPath());
  histogram_tester.ExpectUniqueSample("Windows.TaskbarShortcutMigrationCount",
                                      2, 1);

  profile_props.set_arguments(
      base::StrCat({L"--profile-directory=", non_default_profile_,
                    L" --source-shortcut-location=start-menu"}));
  base::win::ValidateShortcut(profile_shortcut_path, profile_props);
  base::win::ValidateShortcut(uniquified_profile_shortcut_path, profile_props);
  base::win::ValidateShortcut(main_shortcut_path, main_props);
  base::win::ValidateShortcut(fake_profile_shortcut_path, main_props);

  // Subsequent runs should not re-migrate already updated shortcuts.
  MigrateTaskbarPinsCallback(/*pins_path=*/base::FilePath(),
                             /*implicit_apps_path=*/base::FilePath(),
                             /*start_menu_path=*/temp_dir_.GetPath());
  histogram_tester.ExpectBucketCount("Windows.TaskbarShortcutMigrationCount", 0,
                                     1);
}

TEST_F(ShellIntegrationWinMigrateShortcutTest, GetIsPinnedToTaskbar3StateTest) {
  base::ScopedPathOverride exe_override(base::FILE_EXE, chrome_exe_,
                                        /*is_absolute=*/true, /*create=*/false);
  base::ScopedPathOverride taskbar_override(base::DIR_TASKBAR_PINS,
                                            temp_dir_.GetPath());
  base::ScopedPathOverride implicit_override(base::DIR_IMPLICIT_APP_SHORTCUTS,
                                             temp_dir_sub_dir_.GetPath());

  // 1. Initial state: No shortcuts exist -> kNotPinned.
  EXPECT_EQ(IsPinnedToTaskbarResult::kNotPinned, GetIsPinnedToTaskbar3State());

  // 2. Add chrome.exe shortcut to Taskbar directory.
  base::win::ShortcutProperties chrome_props;
  chrome_props.set_target(chrome_exe_);
  ASSERT_TRUE(base::win::CreateOrUpdateShortcutLink(
      temp_dir_.GetPath().Append(L"Chrome.lnk"), chrome_props,
      base::win::ShortcutOperation::kCreateAlways));

  EXPECT_NE(IsPinnedToTaskbarResult::kPinned, GetIsPinnedToTaskbar3State());
}

TEST_F(ShellIntegrationWinMigrateShortcutTest,
       GetIsPinnedToTaskbar3StateFailuresTest) {
  // Failure: Both taskbar and implicit app shortcut paths fail to resolve.
  base::ScopedPathOverride exe_override(
      base::FILE_EXE, chrome_exe_, /*is_absolute=*/true, /*create=*/false);
  base::ScopedPathOverride taskbar_override(base::DIR_TASKBAR_PINS,
                                            base::FilePath(),
                                            /*should_skip_check=*/true);
  base::ScopedPathOverride implicit_override(base::DIR_IMPLICIT_APP_SHORTCUTS,
                                             base::FilePath(),
                                             /*should_skip_check=*/true);

  EXPECT_EQ(IsPinnedToTaskbarResult::kFailure, GetIsPinnedToTaskbar3State());
}

TEST(ShellIntegrationWinTest, GetAppModelIdForProfileTest) {
  const std::wstring base_app_id(install_static::GetBaseAppId());

  // Empty profile path should get chrome::kBrowserAppID
  std::wstring app_name = L"app";
  std::wstring expected_model_id_without_profile =
      base_app_id + L"." + app_name;
  base::FilePath empty_path;
  EXPECT_EQ(expected_model_id_without_profile,
            GetAppUserModelIdForApp(app_name, empty_path));

  // Default profile path should get chrome::kBrowserAppID
  base::FilePath default_user_data_dir;
  chrome::GetDefaultUserDataDirectory(&default_user_data_dir);
  base::FilePath default_profile_path =
      default_user_data_dir.AppendASCII(chrome::kInitialProfile);
  EXPECT_EQ(expected_model_id_without_profile,
            GetAppUserModelIdForApp(app_name, default_profile_path));

  // Non-default profile path should get chrome::kBrowserAppID joined with
  // profile info.
  base::FilePath profile_path(FILE_PATH_LITERAL("root"));
  profile_path = profile_path.Append(FILE_PATH_LITERAL("udd"));
  profile_path = profile_path.Append(FILE_PATH_LITERAL("User Data - Test"));
  EXPECT_EQ(expected_model_id_without_profile + L".udd.UserDataTest",
            GetAppUserModelIdForApp(app_name, profile_path));
}

}  // namespace win

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
TEST(ShellIntegrationWinTest, GetDirectLaunchUrlScheme) {
  std::string scheme = GetDirectLaunchUrlScheme();
  // For branded builds, the scheme should either be "google-chrome"
  // (primary install mode) or empty (secondary/side-by-side install modes)
  // for security reasons.
  if (install_static::InstallDetails::Get().is_primary_mode()) {
    EXPECT_EQ(scheme, "google-chrome");
  } else {
    EXPECT_EQ(scheme, std::string());
  }
}
#else  // !BUILDFLAG(GOOGLE_CHROME_BRANDING)
TEST(ShellIntegrationWinTest, GetDirectLaunchUrlSchemeUnbranded) {
  EXPECT_EQ("chromium", GetDirectLaunchUrlScheme());
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

}  // namespace shell_integration
