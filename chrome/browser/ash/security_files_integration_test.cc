// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <grp.h>
#include <pwd.h>
#include <sys/stat.h>

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/chromeos/crosier/ash_integration_test.h"
#include "chrome/test/base/chromeos/crosier/chromeos_integration_login_mixin.h"

namespace {

// Returns the Posix file permissions for a file at `path`. The file is assumed
// to exist.
int GetFilePermissions(const base::FilePath& path) {
  int mode = 0;
  CHECK(base::GetPosixFilePermissions(path, &mode));
  return mode;
}

// Returns the owner of the file at `path`. The file is assumed to exist.
std::string GetFileOwner(const base::FilePath& path) {
  struct stat info;
  int rv = stat(path.MaybeAsASCII().c_str(), &info);
  CHECK_EQ(rv, 0);
  // Look up the owner in the passwd database.
  struct passwd* file_owner = getpwuid(info.st_uid);
  CHECK(file_owner);
  return file_owner->pw_name;
}

// Returns the group of the file at `path`. The file is assumed to exist.
std::string GetFileGroup(const base::FilePath& path) {
  struct stat info;
  int rv = stat(path.MaybeAsASCII().c_str(), &info);
  CHECK_EQ(rv, 0);
  // Look up the file's group in the group database.
  struct group* file_group = getgrgid(info.st_gid);
  CHECK(file_group);
  return file_group->gr_name;
}

class SecurityFilesIntegrationTest : public AshIntegrationTest {
 public:
  SecurityFilesIntegrationTest() {
    // Keep test running after dismissing login screen.
    set_exit_when_last_browser_closes(false);

    login_mixin().SetMode(ChromeOSIntegrationLoginMixin::Mode::kTestLogin);
  }

  ~SecurityFilesIntegrationTest() override = default;

  // AshIntegrationTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    AshIntegrationTest::SetUpCommandLine(command_line);
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kUserDataDir, "/home/chronos");
  }
};

IN_PROC_BROWSER_TEST_F(SecurityFilesIntegrationTest, UserFilesLoggedIn) {
  login_mixin().Login();

  ash::test::WaitForPrimaryUserSessionStart();
  EXPECT_TRUE(login_mixin().IsCryptohomeMounted());

  base::ScopedAllowBlockingForTesting allow_blocking;

  // Validate /home/chronos.
  base::FilePath home_chronos("/home/chronos");
  ASSERT_TRUE(base::PathExists(home_chronos));
  EXPECT_EQ(GetFilePermissions(home_chronos), 0755);
  EXPECT_EQ(GetFileOwner(home_chronos), "chronos");

  // Validate /home/chronos/user.
  base::FilePath home_chronos_user("/home/chronos/user");
  ASSERT_TRUE(base::PathExists(home_chronos_user));
  EXPECT_FALSE(GetFilePermissions(home_chronos_user) &
               base::FILE_PERMISSION_WRITE_BY_OTHERS);
  EXPECT_EQ(GetFileOwner(home_chronos_user), "chronos");

  // There is at least one directory matching "u-*".
  base::FileEnumerator enumerator(home_chronos, /*recursive=*/false,
                                  base::FileEnumerator::DIRECTORIES, "u-*");
  int count = 0;
  enumerator.ForEach([&count](const base::FilePath& item) {
    count++;
    EXPECT_FALSE(GetFilePermissions(item) &
                 base::FILE_PERMISSION_WRITE_BY_OTHERS);
  });
  EXPECT_GT(count, 0);

  // Validate contents of /home/chronos.
  base::FileEnumerator enumerator2(
      home_chronos, /*recursive=*/false,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES, "*");
  enumerator2.ForEach([](const base::FilePath& item) {
    // /home/chronos/crash is not other writable.
    if (item == base::FilePath("/home/chronos/crash")) {
      EXPECT_FALSE(GetFilePermissions(item) &
                   base::FILE_PERMISSION_WRITE_BY_OTHERS);
      return;
    }
    // All other subdirectories and files are not group writable and not other
    // writable.
    constexpr int banned = base::FILE_PERMISSION_WRITE_BY_GROUP |
                           base::FILE_PERMISSION_WRITE_BY_OTHERS;
    EXPECT_FALSE(GetFilePermissions(item) & banned) << item.MaybeAsASCII();
  });

  // Validate Downloads.
  base::FilePath downloads("/home/chronos/user/MyFiles/Downloads");
  ASSERT_TRUE(base::PathExists(downloads));
  EXPECT_EQ(GetFilePermissions(downloads), 0750);
  EXPECT_EQ(GetFileOwner(downloads), "chronos");
  EXPECT_EQ(GetFileGroup(downloads), "chronos-access");
}

}  // namespace
