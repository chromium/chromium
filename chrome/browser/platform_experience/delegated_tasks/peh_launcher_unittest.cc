// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/platform_experience/delegated_tasks/peh_launcher.h"

#include <memory>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/test/scoped_path_override.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/platform_experience/delegated_tasks/peh_switches.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/install_static/install_util.h"
#include "chrome/install_static/test/scoped_install_details.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace platform_experience {

namespace {

class PehLauncherTest : public testing::Test {
 protected:
  void SetUp() override {
    user_data_dir_override_ =
        std::make_unique<base::ScopedPathOverride>(chrome::DIR_USER_DATA);
    exe_dir_override_ =
        std::make_unique<base::ScopedPathOverride>(base::DIR_EXE);
  }

  void TearDown() override {
    exe_dir_override_.reset();
    user_data_dir_override_.reset();
  }

  void CreateFakeHelperExecutable(bool is_system_install) {
    const int path_key = is_system_install
                             ? static_cast<int>(base::DIR_EXE)
                             : static_cast<int>(chrome::DIR_USER_DATA);
    base::FilePath peh_dir = base::PathService::CheckedGet(path_key).Append(
        L"PlatformExperienceHelper");
    ASSERT_TRUE(base::CreateDirectory(peh_dir));
    base::FilePath peh_exe_path =
        peh_dir.Append(L"platform_experience_helper.exe");
    ASSERT_TRUE(base::WriteFile(peh_exe_path, ""));
  }

  std::unique_ptr<base::ScopedPathOverride> user_data_dir_override_;
  std::unique_ptr<base::ScopedPathOverride> exe_dir_override_;
};

TEST_F(PehLauncherTest, GetBinaryPathUserInstall_NotFound) {
  install_static::ScopedInstallDetails scoped_install_details(
      /*system_level=*/false);

  PehLauncher launcher;
  base::FilePath path = launcher.GetBinaryPath();

  EXPECT_TRUE(path.empty());
}

TEST_F(PehLauncherTest, GetBinaryPathUserInstall_Found) {
  install_static::ScopedInstallDetails scoped_install_details(
      /*system_level=*/false);
  CreateFakeHelperExecutable(/*is_system_install=*/false);

  PehLauncher launcher;
  base::FilePath path = launcher.GetBinaryPath();

  base::FilePath expected_path =
      base::PathService::CheckedGet(chrome::DIR_USER_DATA)
          .Append(L"PlatformExperienceHelper")
          .Append(L"platform_experience_helper.exe");

  EXPECT_EQ(path, expected_path);
}

TEST_F(PehLauncherTest, GetBinaryPathSystemInstall_NotFound) {
  install_static::ScopedInstallDetails scoped_install_details(
      /*system_level=*/true);

  PehLauncher launcher;
  base::FilePath path = launcher.GetBinaryPath();

  EXPECT_TRUE(path.empty());
}

TEST_F(PehLauncherTest, GetBinaryPathSystemInstall_Found) {
  install_static::ScopedInstallDetails scoped_install_details(
      /*system_level=*/true);
  CreateFakeHelperExecutable(/*is_system_install=*/true);

  PehLauncher launcher;
  base::FilePath path = launcher.GetBinaryPath();

  base::FilePath expected_path = base::PathService::CheckedGet(base::DIR_EXE)
                                     .Append(L"PlatformExperienceHelper")
                                     .Append(L"platform_experience_helper.exe");

  EXPECT_EQ(path, expected_path);
}

TEST_F(PehLauncherTest, IsBinaryVerified) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath temp_file = temp_dir.GetPath().Append(L"fake.exe");
  ASSERT_TRUE(base::WriteFile(temp_file, ""));

  PehLauncher launcher;
#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && defined(NDEBUG)
  EXPECT_FALSE(launcher.IsBinaryVerified(temp_file));
#else
  EXPECT_TRUE(launcher.IsBinaryVerified(temp_file));
#endif
}

TEST_F(PehLauncherTest, GetBinaryVersion_NotFound) {
  PehLauncher launcher;
  base::Version version =
      launcher.GetBinaryVersion(base::FilePath(L"does_not_exist.exe"));
  EXPECT_FALSE(version.IsValid());
}

}  // namespace

}  // namespace platform_experience
