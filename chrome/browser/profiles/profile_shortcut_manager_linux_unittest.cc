// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_shortcut_manager_linux.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/scoped_environment_variable_override.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_init_params.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/shell_integration_linux.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Returns the value of `key` in the [Desktop Entry] section of `contents`.
std::string GetDesktopEntryValue(const std::string& key,
                                 const std::string& contents) {
  return shell_integration_linux::internal::
      GetDesktopEntryStringValueFromFromDesktopFileForTest(key, contents);
}

SkBitmap MakeSolidBitmap(int size, SkColor color) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(size, size);
  bitmap.eraseColor(color);
  return bitmap;
}

}  // namespace

class ProfileShortcutManagerLinuxTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    config_home_override_.emplace("XDG_CONFIG_HOME",
                                  temp_dir_.GetPath().Append("config").value());
    data_home_override_.emplace("XDG_DATA_HOME",
                                temp_dir_.GetPath().Append("data").value());
    chrome_config_home_override_.emplace("CHROME_CONFIG_HOME");
    chrome_desktop_override_.emplace("CHROME_DESKTOP");

    ASSERT_TRUE(chrome::GetDefaultUserDataDirectory(&user_data_dir_));
    ASSERT_TRUE(base::CreateDirectory(user_data_dir_));
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp(user_data_dir_));
    storage_ = profile_manager_->profile_attributes_storage();

    default_profile_path_ = AddProfile(chrome::kInitialProfile, u"Personal");
    profile_path_ = AddProfile("Profile 1", u"Work");
    desktop_id_ =
        shell_integration_linux::GetPerProfileDesktopId(profile_path_);
    ASSERT_FALSE(desktop_id_.empty());
    desktop_entry_path_ = temp_dir_.GetPath()
                              .Append("data")
                              .Append("applications")
                              .Append(desktop_id_ + ".desktop");

    // ChromeUnitTestSuite disables the ProfileManager's own shortcut manager,
    // so this is the only instance acting on the desktop entries.
    ASSERT_FALSE(
        profile_manager_->profile_manager()->profile_shortcut_manager());
    shortcut_manager_ = std::make_unique<ProfileShortcutManagerLinux>(
        profile_manager_->profile_manager());
  }

  void TearDown() override {
    Flush();
    shortcut_manager_.reset();
    shell_integration_linux::SetPerProfileDesktopEntryInstalled(profile_path_,
                                                                false);
  }

  base::FilePath AddProfile(const std::string& directory,
                            const std::u16string& name) {
    const base::FilePath path = user_data_dir_.Append(directory);
    EXPECT_TRUE(base::CreateDirectory(path));
    ProfileAttributesInitParams params;
    params.profile_path = path;
    params.profile_name = name;
    storage_->AddProfile(std::move(params));
    return path;
  }

  ProfileAttributesEntry* GetEntry() {
    return storage_->GetProfileAttributesWithPath(profile_path_);
  }

  std::string ReadDesktopEntry() {
    std::string contents;
    EXPECT_TRUE(base::ReadFileToString(desktop_entry_path_, &contents));
    return contents;
  }

  // Returns the icon referenced by the desktop entry.
  base::FilePath GetIconPath() {
    return base::FilePath(GetDesktopEntryValue("Icon", ReadDesktopEntry()));
  }

  bool HasProfileShortcuts(const base::FilePath& profile_path) {
    base::test::TestFuture<bool> future;
    shortcut_manager_->HasProfileShortcuts(profile_path, future.GetCallback());
    return future.Get();
  }

  // Waits until all pending file operations and their replies have completed.
  // Replies are posted to this sequence in order, so the replies of earlier
  // operations have run once the flush reply runs.
  // Also waits for newly installed entries to be registered.
  void Flush() {
    FlushFileOperations();
    task_environment_.FastForwardBy(
        ProfileShortcutManagerLinux::kDesktopEntryRegistrationDelay);
  }

  void FlushFileOperations() {
    base::RunLoop run_loop;
    shortcut_manager_->FlushForTesting(run_loop.QuitClosure());
    run_loop.Run();
  }

  std::u16string ExpectedName(const std::u16string& profile_name) {
    return base::StrCat({l10n_util::GetStringUTF16(IDS_PRODUCT_NAME), u" (",
                         profile_name, u")"});
  }

  base::test::ScopedFeatureList feature_list_{features::kLinuxPerProfileAppId};
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::ScopedTempDir temp_dir_;
  std::optional<base::ScopedEnvironmentVariableOverride> config_home_override_;
  std::optional<base::ScopedEnvironmentVariableOverride> data_home_override_;
  std::optional<base::ScopedEnvironmentVariableOverride>
      chrome_config_home_override_;
  std::optional<base::ScopedEnvironmentVariableOverride>
      chrome_desktop_override_;
  base::FilePath user_data_dir_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  raw_ptr<ProfileAttributesStorage> storage_ = nullptr;
  base::FilePath default_profile_path_;
  base::FilePath profile_path_;
  std::string desktop_id_;
  base::FilePath desktop_entry_path_;
  std::unique_ptr<ProfileShortcutManagerLinux> shortcut_manager_;
};

TEST_F(ProfileShortcutManagerLinuxTest, CreateAndRemoveShortcut) {
  EXPECT_TRUE(shortcut_manager_->CanCreateProfileShortcut(profile_path_));
  EXPECT_FALSE(HasProfileShortcuts(profile_path_));

  shortcut_manager_->CreateProfileShortcut(profile_path_);
  Flush();

  ASSERT_TRUE(base::PathExists(desktop_entry_path_));
  const std::string contents = ReadDesktopEntry();
  EXPECT_EQ(base::UTF16ToUTF8(ExpectedName(u"Work")),
            GetDesktopEntryValue("Name", contents));
  // StartupWMClass must match the WM_CLASS res_name / app_id of the profile's
  // browser windows.
  EXPECT_EQ(desktop_id_, GetDesktopEntryValue("StartupWMClass", contents));
  // URLs dropped onto the launcher are passed to the browser.
  EXPECT_TRUE(base::EndsWith(GetDesktopEntryValue("Exec", contents),
                             " \"--profile-directory=Profile 1\" %U"));
  // No MIME types, so the entry doesn't compete with the main browser entry.
  EXPECT_EQ("", GetDesktopEntryValue("MimeType", contents));
  const base::FilePath icon_path = GetIconPath();
  EXPECT_EQ(profile_path_, icon_path.DirName());
  EXPECT_TRUE(
      base::StartsWith(icon_path.BaseName().value(),
                       profiles::internal::kProfileDesktopEntryIconPrefix));
  EXPECT_TRUE(base::PathExists(icon_path));

  EXPECT_TRUE(HasProfileShortcuts(profile_path_));
  EXPECT_EQ(
      desktop_id_,
      shell_integration_linux::GetInstalledPerProfileDesktopId(profile_path_));

  shortcut_manager_->RemoveProfileShortcuts(profile_path_);
  Flush();

  EXPECT_FALSE(base::PathExists(desktop_entry_path_));
  EXPECT_FALSE(base::PathExists(icon_path));
  EXPECT_FALSE(HasProfileShortcuts(profile_path_));
  EXPECT_EQ("", shell_integration_linux::GetInstalledPerProfileDesktopId(
                    profile_path_));
}

TEST_F(ProfileShortcutManagerLinuxTest, RegistrationIsDelayed) {
  shortcut_manager_->CreateProfileShortcut(profile_path_);
  FlushFileOperations();
  ASSERT_TRUE(base::PathExists(desktop_entry_path_));

  // Windows only switch to the entry once the desktop environment had time to
  // pick it up.
  EXPECT_EQ("", shell_integration_linux::GetInstalledPerProfileDesktopId(
                    profile_path_));
  task_environment_.FastForwardBy(
      ProfileShortcutManagerLinux::kDesktopEntryRegistrationDelay -
      base::Seconds(1));
  EXPECT_EQ("", shell_integration_linux::GetInstalledPerProfileDesktopId(
                    profile_path_));
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_EQ(
      desktop_id_,
      shell_integration_linux::GetInstalledPerProfileDesktopId(profile_path_));
}

TEST_F(ProfileShortcutManagerLinuxTest, RemoveDuringRegistrationDelay) {
  shortcut_manager_->CreateProfileShortcut(profile_path_);
  FlushFileOperations();
  shortcut_manager_->RemoveProfileShortcuts(profile_path_);
  Flush();

  EXPECT_FALSE(base::PathExists(desktop_entry_path_));
  EXPECT_EQ("", shell_integration_linux::GetInstalledPerProfileDesktopId(
                    profile_path_));
}

TEST_F(ProfileShortcutManagerLinuxTest, NoShortcutForDefaultProfile) {
  EXPECT_FALSE(
      shortcut_manager_->CanCreateProfileShortcut(default_profile_path_));

  shortcut_manager_->CreateProfileShortcut(default_profile_path_);
  Flush();

  EXPECT_FALSE(base::PathExists(
      temp_dir_.GetPath().Append("data").Append("applications")));
  EXPECT_FALSE(HasProfileShortcuts(default_profile_path_));
  EXPECT_EQ("", shell_integration_linux::GetInstalledPerProfileDesktopId(
                    default_profile_path_));
}

TEST_F(ProfileShortcutManagerLinuxTest, RenameUpdatesShortcut) {
  shortcut_manager_->CreateProfileShortcut(profile_path_);
  Flush();
  ASSERT_TRUE(base::PathExists(desktop_entry_path_));

  GetEntry()->SetLocalProfileName(u"Office", /*is_default_name=*/false);
  Flush();

  EXPECT_EQ(base::UTF16ToUTF8(ExpectedName(u"Office")),
            GetDesktopEntryValue("Name", ReadDesktopEntry()));
}

TEST_F(ProfileShortcutManagerLinuxTest, RenameDoesNotCreateShortcut) {
  GetEntry()->SetLocalProfileName(u"Office", /*is_default_name=*/false);
  Flush();

  EXPECT_FALSE(base::PathExists(desktop_entry_path_));
  EXPECT_EQ("", shell_integration_linux::GetInstalledPerProfileDesktopId(
                    profile_path_));
}

TEST_F(ProfileShortcutManagerLinuxTest, ProfileDeletionRemovesShortcut) {
  shortcut_manager_->CreateProfileShortcut(profile_path_);
  Flush();
  ASSERT_TRUE(base::PathExists(desktop_entry_path_));

  storage_->RemoveProfile(profile_path_);
  Flush();

  EXPECT_FALSE(base::PathExists(desktop_entry_path_));
  EXPECT_EQ("", shell_integration_linux::GetInstalledPerProfileDesktopId(
                    profile_path_));
}

TEST_F(ProfileShortcutManagerLinuxTest, RemoveWhileCreating) {
  // Removing the shortcut before the write has completed must not leave a
  // desktop entry behind, or mark the profile as having one.
  shortcut_manager_->CreateProfileShortcut(profile_path_);
  shortcut_manager_->RemoveProfileShortcuts(profile_path_);
  Flush();

  EXPECT_FALSE(base::PathExists(desktop_entry_path_));
  EXPECT_EQ("", shell_integration_linux::GetInstalledPerProfileDesktopId(
                    profile_path_));
}

TEST_F(ProfileShortcutManagerLinuxTest, RemoveCreateWhileCreating) {
  // Only the last operation decides whether the profile has an entry.
  shortcut_manager_->CreateProfileShortcut(profile_path_);
  shortcut_manager_->RemoveProfileShortcuts(profile_path_);
  shortcut_manager_->CreateProfileShortcut(profile_path_);
  Flush();

  EXPECT_TRUE(base::PathExists(desktop_entry_path_));
  EXPECT_EQ(
      desktop_id_,
      shell_integration_linux::GetInstalledPerProfileDesktopId(profile_path_));
}

TEST_F(ProfileShortcutManagerLinuxTest, AvatarChangeChangesIconPath) {
  shortcut_manager_->CreateProfileShortcut(profile_path_);
  Flush();
  const base::FilePath old_icon_path = GetIconPath();
  ASSERT_TRUE(base::PathExists(old_icon_path));

  // Desktop environments cache icons by path, so a new icon must get a new
  // path, and the old icon is cleaned up.
  GetEntry()->SetAvatarIconIndex(GetEntry()->GetAvatarIconIndex() == 1 ? 2 : 1);
  Flush();

  const base::FilePath new_icon_path = GetIconPath();
  EXPECT_NE(old_icon_path, new_icon_path);
  EXPECT_TRUE(base::PathExists(new_icon_path));
  EXPECT_FALSE(base::PathExists(old_icon_path));
}

TEST_F(ProfileShortcutManagerLinuxTest, ExternallyDeletedEntryIsNotRecreated) {
  shortcut_manager_->CreateProfileShortcut(profile_path_);
  Flush();
  const base::FilePath icon_path = GetIconPath();
  ASSERT_TRUE(base::PathExists(icon_path));
  ASSERT_TRUE(base::DeleteFile(desktop_entry_path_));

  GetEntry()->SetLocalProfileName(u"Office", /*is_default_name=*/false);
  Flush();

  EXPECT_FALSE(base::PathExists(desktop_entry_path_));
  // The icon of the deleted entry is cleaned up.
  EXPECT_FALSE(base::PathExists(icon_path));
  EXPECT_TRUE(GetEntry()->GetShortcutName().empty());
  EXPECT_EQ("", shell_integration_linux::GetInstalledPerProfileDesktopId(
                    profile_path_));
}

TEST_F(ProfileShortcutManagerLinuxTest, LastProfileShortcutIsRemoved) {
  shortcut_manager_->CreateProfileShortcut(profile_path_);
  Flush();
  ASSERT_TRUE(base::PathExists(desktop_entry_path_));

  // The shortcut setting is hidden when only one profile is left, so its
  // entry must not be left behind.
  storage_->RemoveProfile(default_profile_path_);
  Flush();

  EXPECT_FALSE(base::PathExists(desktop_entry_path_));
  EXPECT_TRUE(GetEntry()->GetShortcutName().empty());
  EXPECT_EQ("", shell_integration_linux::GetInstalledPerProfileDesktopId(
                    profile_path_));
}

TEST(ProfileShortcutManagerLinuxIconTest, BadgeIconWithAvatar) {
  const SkBitmap app_icon = MakeSolidBitmap(256, SK_ColorRED);
  const SkBitmap avatar = MakeSolidBitmap(128, SK_ColorBLUE);

  const SkBitmap badged =
      profiles::internal::BadgeIconWithAvatar(app_icon, avatar);
  ASSERT_EQ(256, badged.width());
  ASSERT_EQ(256, badged.height());
  // The top left is untouched, the badge is in the bottom right, and there is
  // a transparent ring around the badge.
  EXPECT_EQ(SK_ColorRED, badged.getColor(10, 10));
  EXPECT_EQ(SK_ColorBLUE, badged.getColor(192, 192));
  EXPECT_EQ(SK_ColorTRANSPARENT, SkColorGetA(badged.getColor(192, 125)));

  // Without an avatar, the icon isn't badged.
  const SkBitmap unbadged =
      profiles::internal::BadgeIconWithAvatar(app_icon, SkBitmap());
  EXPECT_EQ(SK_ColorRED, unbadged.getColor(192, 192));

  // Without an app icon, there's nothing to badge.
  EXPECT_TRUE(profiles::internal::BadgeIconWithAvatar(SkBitmap(), avatar)
                  .drawsNothing());
}
