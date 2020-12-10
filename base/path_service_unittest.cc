// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/path_service.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/test/gtest_util.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest-spi.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace base {

namespace {

// Returns true if PathService::Get returns true and sets the path parameter
// to non-empty for the given PathService::DirType enumeration value.
bool ReturnsValidPath(int dir_type) {
  FilePath path;
  bool result = PathService::Get(dir_type, &path);

  // Some paths might not exist on some platforms in which case confirming
  // |result| is true and !path.empty() is the best we can do.
  bool check_path_exists = true;
#if defined(OS_POSIX)
  // If chromium has never been started on this account, the cache path may not
  // exist.
  if (dir_type == DIR_CACHE)
    check_path_exists = false;
#endif
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  // On the linux try-bots: a path is returned (e.g. /home/chrome-bot/Desktop),
  // but it doesn't exist.
  if (dir_type == DIR_USER_DESKTOP)
    check_path_exists = false;
#endif
#if defined(OS_WIN)
  if (dir_type == DIR_TASKBAR_PINS)
    check_path_exists = false;
#endif
#if defined(OS_APPLE)
  if (dir_type != DIR_EXE && dir_type != DIR_MODULE && dir_type != FILE_EXE &&
      dir_type != FILE_MODULE) {
    if (path.ReferencesParent()) {
      LOG(INFO) << "Path (" << path << ") references parent.";
      return false;
    }
  }
#else
  if (path.ReferencesParent()) {
    LOG(INFO) << "Path (" << path << ") references parent.";
    return false;
  }
#endif
  if (!result) {
    LOG(INFO) << "PathService::Get() returned false.";
    return false;
  }
  if (path.empty()) {
    LOG(INFO) << "PathService::Get() returned an empty path.";
    return false;
  }
  if (check_path_exists && !PathExists(path)) {
    LOG(INFO) << "Path (" << path << ") does not exist.";
    return false;
  }
  return true;
}

#if defined(OS_WIN)
// Function to test any directory keys that are not supported on some versions
// of Windows. Checks that the function fails and that the returned path is
// empty.
bool ReturnsInvalidPath(int dir_type) {
  FilePath path;
  bool result = PathService::Get(dir_type, &path);
  return !result && path.empty();
}
#endif

}  // namespace

// On the Mac this winds up using some autoreleased objects, so we need to
// be a PlatformTest.
typedef PlatformTest PathServiceTest;

// Test that all PathService::Get calls return a value and a true result
// in the development environment.  (This test was created because a few
// later changes to Get broke the semantics of the function and yielded the
// correct value while returning false.)
TEST_F(PathServiceTest, Get) {
  for (int key = PATH_START + 1; key < PATH_END; ++key) {
#if defined(OS_ANDROID)
    if (key == FILE_MODULE || key == DIR_USER_DESKTOP ||
        key == DIR_HOME)
      continue;  // Android doesn't implement these.
#elif defined(OS_IOS)
    if (key == DIR_USER_DESKTOP)
      continue;  // iOS doesn't implement DIR_USER_DESKTOP.
#elif defined(OS_FUCHSIA)
    if (key == DIR_USER_DESKTOP || key == FILE_MODULE || key == DIR_MODULE)
      continue;  // Fuchsia doesn't implement DIR_USER_DESKTOP, FILE_MODULE and
                 // DIR_MODULE.
#endif
    EXPECT_PRED1(ReturnsValidPath, key);
  }
#if defined(OS_WIN)
  for (int key = PATH_WIN_START + 1; key < PATH_WIN_END; ++key) {
    bool valid = true;
    if (key == DIR_APP_SHORTCUTS)
      valid = base::win::GetVersion() >= base::win::Version::WIN8;

    if (valid)
      EXPECT_PRED1(ReturnsValidPath, key);
    else
      EXPECT_PRED1(ReturnsInvalidPath, key);
  }
#elif defined(OS_APPLE)
  for (int key = PATH_MAC_START + 1; key < PATH_MAC_END; ++key) {
    EXPECT_PRED1(ReturnsValidPath, key);
  }
#elif defined(OS_ANDROID)
  for (int key = PATH_ANDROID_START + 1; key < PATH_ANDROID_END;
       ++key) {
    EXPECT_PRED1(ReturnsValidPath, key);
  }
#elif defined(OS_POSIX)
  for (int key = PATH_POSIX_START + 1; key < PATH_POSIX_END;
       ++key) {
    EXPECT_PRED1(ReturnsValidPath, key);
  }
#endif
}

// Tests that CheckedGet returns the same path as Get.
TEST_F(PathServiceTest, CheckedGet) {
  constexpr int kKey = DIR_CURRENT;
  FilePath path;
  ASSERT_TRUE(PathService::Get(kKey, &path));
  EXPECT_EQ(path, PathService::CheckedGet(kKey));
}

#if defined(GTEST_HAS_DEATH_TEST)

// Tests that CheckedGet CHECKs on failure.
TEST_F(PathServiceTest, CheckedGetFailure) {
  constexpr int kBadKey = PATH_END;
  FilePath path;
  EXPECT_FALSE(PathService::Get(kBadKey, &path));
  EXPECT_DEATH(PathService::CheckedGet(kBadKey), "Failed to get the path");
}

#endif  // GTEST_HAS_DEATH_TEST

// Test that all versions of the Override function of PathService do what they
// are supposed to do.
TEST_F(PathServiceTest, Override) {
  int my_special_key = 666;
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  FilePath fake_cache_dir(temp_dir.GetPath().AppendASCII("cache"));
  // PathService::Override should always create the path provided if it doesn't
  // exist.
  EXPECT_TRUE(PathService::Override(my_special_key, fake_cache_dir));
  EXPECT_TRUE(PathExists(fake_cache_dir));

  FilePath fake_cache_dir2(temp_dir.GetPath().AppendASCII("cache2"));
  // PathService::OverrideAndCreateIfNeeded should obey the |create| parameter.
  PathService::OverrideAndCreateIfNeeded(my_special_key,
                                         fake_cache_dir2,
                                         false,
                                         false);
  EXPECT_FALSE(PathExists(fake_cache_dir2));
  EXPECT_TRUE(PathService::OverrideAndCreateIfNeeded(my_special_key,
                                                     fake_cache_dir2,
                                                     false,
                                                     true));
  EXPECT_TRUE(PathExists(fake_cache_dir2));

#if defined(OS_POSIX)
  FilePath non_existent(
      MakeAbsoluteFilePath(temp_dir.GetPath()).AppendASCII("non_existent"));
  EXPECT_TRUE(non_existent.IsAbsolute());
  EXPECT_FALSE(PathExists(non_existent));
#if !defined(OS_ANDROID)
  // This fails because MakeAbsoluteFilePath fails for non-existent files.
  // Earlier versions of Bionic libc don't fail for non-existent files, so
  // skip this check on Android.
  EXPECT_FALSE(PathService::OverrideAndCreateIfNeeded(my_special_key,
                                                      non_existent,
                                                      false,
                                                      false));
#endif
  // This works because indicating that |non_existent| is absolute skips the
  // internal MakeAbsoluteFilePath call.
  EXPECT_TRUE(PathService::OverrideAndCreateIfNeeded(my_special_key,
                                                     non_existent,
                                                     true,
                                                     false));
  // Check that the path has been overridden and no directory was created.
  EXPECT_FALSE(PathExists(non_existent));
  FilePath path;
  EXPECT_TRUE(PathService::Get(my_special_key, &path));
  EXPECT_EQ(non_existent, path);
#endif
}

// Check if multiple overrides can co-exist.
TEST_F(PathServiceTest, OverrideMultiple) {
  int my_special_key = 666;
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  FilePath fake_cache_dir1(temp_dir.GetPath().AppendASCII("1"));
  EXPECT_TRUE(PathService::Override(my_special_key, fake_cache_dir1));
  EXPECT_TRUE(PathExists(fake_cache_dir1));
  ASSERT_TRUE(WriteFile(fake_cache_dir1.AppendASCII("t1"), "."));

  FilePath fake_cache_dir2(temp_dir.GetPath().AppendASCII("2"));
  EXPECT_TRUE(PathService::Override(my_special_key + 1, fake_cache_dir2));
  EXPECT_TRUE(PathExists(fake_cache_dir2));
  ASSERT_TRUE(WriteFile(fake_cache_dir2.AppendASCII("t2"), "."));

  FilePath result;
  EXPECT_TRUE(PathService::Get(my_special_key, &result));
  // Override might have changed the path representation but our test file
  // should be still there.
  EXPECT_TRUE(PathExists(result.AppendASCII("t1")));
  EXPECT_TRUE(PathService::Get(my_special_key + 1, &result));
  EXPECT_TRUE(PathExists(result.AppendASCII("t2")));
}

TEST_F(PathServiceTest, RemoveOverride) {
  // Before we start the test we have to call RemoveOverride at least once to
  // clear any overrides that might have been left from other tests.
  PathService::RemoveOverrideForTests(DIR_TEMP);

  FilePath original_user_data_dir;
  EXPECT_TRUE(PathService::Get(DIR_TEMP, &original_user_data_dir));
  EXPECT_FALSE(PathService::RemoveOverrideForTests(DIR_TEMP));

  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  EXPECT_TRUE(PathService::Override(DIR_TEMP, temp_dir.GetPath()));
  FilePath new_user_data_dir;
  EXPECT_TRUE(PathService::Get(DIR_TEMP, &new_user_data_dir));
  EXPECT_NE(original_user_data_dir, new_user_data_dir);

  EXPECT_TRUE(PathService::RemoveOverrideForTests(DIR_TEMP));
  EXPECT_TRUE(PathService::Get(DIR_TEMP, &new_user_data_dir));
  EXPECT_EQ(original_user_data_dir, new_user_data_dir);
}

#if defined(OS_WIN)
TEST_F(PathServiceTest, GetProgramFiles) {
  FilePath programfiles_dir;
#if defined(_WIN64)
  // 64-bit on 64-bit.
  EXPECT_TRUE(PathService::Get(DIR_PROGRAM_FILES,
      &programfiles_dir));
  EXPECT_EQ(programfiles_dir.value(),
      FILE_PATH_LITERAL("C:\\Program Files"));
  EXPECT_TRUE(PathService::Get(DIR_PROGRAM_FILESX86,
      &programfiles_dir));
  EXPECT_EQ(programfiles_dir.value(),
      FILE_PATH_LITERAL("C:\\Program Files (x86)"));
  EXPECT_TRUE(PathService::Get(DIR_PROGRAM_FILES6432,
      &programfiles_dir));
  EXPECT_EQ(programfiles_dir.value(),
      FILE_PATH_LITERAL("C:\\Program Files"));
#else
  if (base::win::OSInfo::GetInstance()->wow64_status() ==
      base::win::OSInfo::WOW64_ENABLED) {
    // 32-bit on 64-bit.
    EXPECT_TRUE(PathService::Get(DIR_PROGRAM_FILES,
        &programfiles_dir));
    EXPECT_EQ(programfiles_dir.value(),
        FILE_PATH_LITERAL("C:\\Program Files (x86)"));
    EXPECT_TRUE(PathService::Get(DIR_PROGRAM_FILESX86,
        &programfiles_dir));
    EXPECT_EQ(programfiles_dir.value(),
        FILE_PATH_LITERAL("C:\\Program Files (x86)"));
    EXPECT_TRUE(PathService::Get(DIR_PROGRAM_FILES6432,
        &programfiles_dir));
    EXPECT_EQ(programfiles_dir.value(),
        FILE_PATH_LITERAL("C:\\Program Files"));
  } else {
    // 32-bit on 32-bit.
    EXPECT_TRUE(PathService::Get(DIR_PROGRAM_FILES,
        &programfiles_dir));
    EXPECT_EQ(programfiles_dir.value(),
        FILE_PATH_LITERAL("C:\\Program Files"));
    EXPECT_TRUE(PathService::Get(DIR_PROGRAM_FILESX86,
        &programfiles_dir));
    EXPECT_EQ(programfiles_dir.value(),
        FILE_PATH_LITERAL("C:\\Program Files"));
    EXPECT_TRUE(PathService::Get(DIR_PROGRAM_FILES6432,
        &programfiles_dir));
    EXPECT_EQ(programfiles_dir.value(),
        FILE_PATH_LITERAL("C:\\Program Files"));
  }
#endif
}
#endif

}  // namespace base
