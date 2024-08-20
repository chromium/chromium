// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/path_service.h"

#include "base/base_paths.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/scoped_environment_variable_override.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gtest_util.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest-spi.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if BUILDFLAG(IS_WIN)
#include <shlobj.h>

#include "base/win/windows_version.h"
#endif

#if BUILDFLAG(IS_APPLE)
#include "base/apple/bundle_locations.h"
#endif

namespace base {

namespace {

#if BUILDFLAG(IS_ANDROID)
// Defined in
// //base/test/android/javatests/src/org/chromium/base/test/util/UrlUtils.java.
constexpr char kExpectedChromiumTestsRoot[] =
    "/storage/emulated/0/chromium_tests_root";
#endif

// Returns true if PathService::Get returns true and sets the path parameter
// to non-empty for the given PathService key enumeration value.
bool ReturnsValidPath(int key) {
  FilePath path;
  bool result = PathService::Get(key, &path);

  // Some paths might not exist on some platforms in which case confirming
  // |result| is true and !path.empty() is the best we can do.
  bool check_path_exists = true;

#if BUILDFLAG(IS_POSIX)
  // If chromium has never been started on this account, the cache path may not
  // exist.
  if (key == DIR_CACHE)
    check_path_exists = false;
#endif
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // On the linux try-bots: a path is returned (e.g. /home/chrome-bot/Desktop),
  // but it doesn't exist.
  if (key == DIR_USER_DESKTOP)
    check_path_exists = false;
#endif
#if BUILDFLAG(IS_WIN)
  if (key == DIR_TASKBAR_PINS)
    check_path_exists = false;
#endif
#if BUILDFLAG(IS_MAC)
  if (key != DIR_EXE && key != DIR_MODULE && key != FILE_EXE &&
      key != FILE_MODULE) {
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
#endif  // BUILDFLAG(IS_MAC)
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

// Returns true if PathService::Get returns false and path parameter is empty
// for the given PathService key enumeration value. Used to test path keys that
// are not supported on the platform or on some versions of Windows.
bool ReturnsInvalidPath(int key) {
  FilePath path;
  bool result = PathService::Get(key, &path);
  return !result && path.empty();
}

}  // namespace

// On the Mac this winds up using some autoreleased objects, so we need to
// be a PlatformTest.
typedef PlatformTest PathServiceTest;

// Test that all PathService::Get calls return a value and a true result
// in the development environment.  (This test was created because a few
// later changes to Get broke the semantics of the function and yielded the
// correct value while returning false.)
// If this test fails for specific value(s) on a specific platform, consider not
// defining the enum value on that platform rather than skipping or expecting
// failure for the value(s) on that platform in this test.
TEST_F(PathServiceTest, Get) {
  // Contains keys that are defined but not supported on the platform.
#if BUILDFLAG(IS_ANDROID)
  // The following keys are not intended to be implemented on Android (see
  // crbug.com/1257402). Current implementation is described before each key.
  // TODO(crbug.com/40796336): Remove the definition of these keys on Android
  // or at least fix the behavior of DIR_HOME.
  constexpr std::array kUnsupportedKeys = {
      // Though DIR_HOME is not intended to be supported, PathProviderPosix
      // handles it and returns true. Thus, it is NOT included in the array.
      /* DIR_HOME, */
      // PathProviderAndroid and PathProviderPosix both return false.
      FILE_MODULE,
      // PathProviderPosix handles it but fails at some point.
      DIR_USER_DESKTOP};
#elif BUILDFLAG(IS_FUCHSIA)
  constexpr std::array kUnsupportedKeys = {
      // TODO(crbug.com/42050322): Implement DIR_USER_DESKTOP.
      DIR_USER_DESKTOP};
#else
  constexpr std::array<BasePathKey, 0> kUnsupportedKeys = {};
#endif  // BUILDFLAG(IS_ANDROID)
  for (int key = PATH_START + 1; key < PATH_END; ++key) {
    EXPECT_PRED1(Contains(kUnsupportedKeys, key) ? &ReturnsInvalidPath
                                                 : &ReturnsValidPath,
                 key);
  }
#if BUILDFLAG(IS_WIN)
  for (int key = PATH_WIN_START + 1; key < PATH_WIN_END; ++key) {
    if (key == DIR_SYSTEM_TEMP) {
      EXPECT_PRED1(::IsUserAnAdmin() ? &ReturnsValidPath : &ReturnsInvalidPath,
                   key);
    } else {
      EXPECT_PRED1(ReturnsValidPath, key);
    }
  }
#elif BUILDFLAG(IS_MAC)
  for (int key = PATH_MAC_START + 1; key < PATH_MAC_END; ++key) {
    EXPECT_PRED1(ReturnsValidPath, key);
  }
#elif BUILDFLAG(IS_IOS)
  for (int key = PATH_IOS_START + 1; key < PATH_IOS_END; ++key) {
    EXPECT_PRED1(ReturnsValidPath, key);
  }
#elif BUILDFLAG(IS_ANDROID)
  for (int key = PATH_ANDROID_START + 1; key < PATH_ANDROID_END;
       ++key) {
    EXPECT_PRED1(ReturnsValidPath, key);
  }
#elif BUILDFLAG(IS_POSIX)
  for (int key = PATH_POSIX_START + 1; key < PATH_POSIX_END;
       ++key) {
    EXPECT_PRED1(ReturnsValidPath, key);
  }
#endif  // BUILDFLAG(IS_WIN)
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

#endif  // defined(GTEST_HAS_DEATH_TEST)

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

#if BUILDFLAG(IS_POSIX)
  FilePath non_existent(
      MakeAbsoluteFilePath(temp_dir.GetPath()).AppendASCII("non_existent"));
  EXPECT_TRUE(non_existent.IsAbsolute());
  EXPECT_FALSE(PathExists(non_existent));
#if !BUILDFLAG(IS_ANDROID)
  // This fails because MakeAbsoluteFilePath fails for non-existent files.
  // Earlier versions of Bionic libc don't fail for non-existent files, so
  // skip this check on Android.
  EXPECT_FALSE(PathService::OverrideAndCreateIfNeeded(my_special_key,
                                                      non_existent,
                                                      false,
                                                      false));
#endif  // !BUILDFLAG(IS_ANDROID)
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
#endif  // BUILDFLAG(IS_POSIX)
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

#if BUILDFLAG(IS_WIN)
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
  if (base::win::OSInfo::GetInstance()->IsWowX86OnAMD64() ||
      base::win::OSInfo::GetInstance()->IsWowX86OnARM64()) {
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
#endif  // defined(_WIN64)
}

TEST_F(PathServiceTest, GetSystemTemp) {
  FilePath secure_system_temp;

  EXPECT_EQ(PathService::Get(DIR_SYSTEM_TEMP, &secure_system_temp),
            ::IsUserAnAdmin());
  if (!secure_system_temp.empty()) {
    FilePath dir_windows;
    ASSERT_TRUE(PathService::Get(DIR_WINDOWS, &dir_windows));
    FilePath dir_program_files;
    ASSERT_TRUE(PathService::Get(DIR_PROGRAM_FILES, &dir_program_files));

    ASSERT_TRUE((dir_windows.AppendASCII("SystemTemp") == secure_system_temp) ||
                (dir_program_files == secure_system_temp));
  }
}
#endif  // BUILDFLAG(IS_WIN)

// Tests that DIR_ASSETS is
// - the package root on Fuchsia,
// - overridden in tests by test_support_android.cc,
// - equals to base::apple::FrameworkBundlePath() on iOS,
// - a sub-directory of base::apple::FrameworkBundlePath() on iOS catalyst,
// - equals to DIR_MODULE otherwise.
TEST_F(PathServiceTest, DIR_ASSETS) {
  FilePath path;
  ASSERT_TRUE(PathService::Get(DIR_ASSETS, &path));
#if BUILDFLAG(IS_FUCHSIA)
  EXPECT_EQ(path.value(), "/pkg");
#elif BUILDFLAG(IS_ANDROID)
  // This key is overridden in //base/test/test_support_android.cc.
  EXPECT_EQ(path.value(), kExpectedChromiumTestsRoot);
#elif BUILDFLAG(IS_IOS_MACCATALYST)
  EXPECT_TRUE(base::apple::FrameworkBundlePath().IsParent(path));
#elif BUILDFLAG(IS_IOS)
  EXPECT_EQ(path, base::apple::FrameworkBundlePath());
#else
  EXPECT_EQ(path, PathService::CheckedGet(DIR_MODULE));
#endif
}

// DIR_OUT_TEST_DATA_ROOT is DIR_MODULE except on Fuchsia where it is the
// package root, on ios where it is the resources directory and on Android
// where it is overridden in tests by test_support_android.cc.
TEST_F(PathServiceTest, DIR_OUT_TEST_DATA_ROOT) {
  FilePath path;
  ASSERT_TRUE(PathService::Get(DIR_OUT_TEST_DATA_ROOT, &path));
#if BUILDFLAG(IS_FUCHSIA)
  EXPECT_EQ(path.value(), "/pkg");
#elif BUILDFLAG(IS_ANDROID)
  // This key is overridden in //base/test/test_support_android.cc.
  EXPECT_EQ(path.value(), kExpectedChromiumTestsRoot);
#elif BUILDFLAG(IS_IOS)
  // On iOS, build output files are moved to the resources directory.
  EXPECT_EQ(path, base::apple::FrameworkBundlePath());
#else
  // On other platforms all build output is in the same directory,
  // so DIR_OUT_TEST_DATA_ROOT should match DIR_MODULE.
  EXPECT_EQ(path, PathService::CheckedGet(DIR_MODULE));
#endif
}

// Test that DIR_GEN_TEST_DATA_ROOT contains dummy_generated.txt which is
// generated for this test.
TEST_F(PathServiceTest, DIR_GEN_TEST_DATA_ROOT) {
  FilePath path;
  ASSERT_TRUE(PathService::Get(DIR_GEN_TEST_DATA_ROOT, &path));
  EXPECT_TRUE(base::PathExists(
      path.Append(FILE_PATH_LITERAL("base/generated_file_for_test.txt"))));
}

#if ((BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_APPLE) && \
      !BUILDFLAG(IS_ANDROID)) ||                     \
     BUILDFLAG(IS_WIN))

// Test that CR_SOURCE_ROOT is being used when set.
// By default on those platforms, this directory is set to two directories up
// the current executable directory ("../../").
TEST_F(PathServiceTest, SetTestDataRootAsAbsolutePath) {
  // This is needed because on some platform `DIR_SRC_TEST_DATA_ROOT` can be
  // cached before reaching this function.
  PathService::DisableCache();
  base::ScopedTempDir tempdir;
  ASSERT_TRUE(tempdir.CreateUniqueTempDir());

#if BUILDFLAG(IS_WIN)
  auto scoped_env = base::ScopedEnvironmentVariableOverride(
      "CR_SOURCE_ROOT", base::WideToUTF8(tempdir.GetPath().value()));
#else
  auto scoped_env = base::ScopedEnvironmentVariableOverride(
      "CR_SOURCE_ROOT", tempdir.GetPath().value());
#endif

  base::FilePath test_data_root;
  ASSERT_TRUE(PathService::Get(DIR_SRC_TEST_DATA_ROOT, &test_data_root));

  ASSERT_EQ(test_data_root, tempdir.GetPath());
}

// Test that CR_SOURCE_ROOT is being used when set.
TEST_F(PathServiceTest, SetTestDataRootAsRelativePath) {
  // This is needed because on some platform `DIR_SRC_TEST_DATA_ROOT` can be
  // cached before reaching this function.
  PathService::DisableCache();
#if BUILDFLAG(IS_WIN)
  auto scoped_env = base::ScopedEnvironmentVariableOverride(
      "CR_SOURCE_ROOT", base::WideToUTF8(base::FilePath::kParentDirectory));
#else
  auto scoped_env = base::ScopedEnvironmentVariableOverride(
      "CR_SOURCE_ROOT", base::FilePath::kParentDirectory);
#endif
  base::FilePath path;
  ASSERT_TRUE(PathService::Get(DIR_EXE, &path));

  base::FilePath test_data_root;
  ASSERT_TRUE(PathService::Get(DIR_SRC_TEST_DATA_ROOT, &test_data_root));

  path = MakeAbsoluteFilePath(path.Append(base::FilePath::kParentDirectory));
  ASSERT_EQ(test_data_root, path);
}

#endif

#if BUILDFLAG(IS_FUCHSIA)
// On Fuchsia, some keys have fixed paths that are easy to test.

TEST_F(PathServiceTest, DIR_SRC_TEST_DATA_ROOT) {
  FilePath test_binary_path;
  EXPECT_EQ(PathService::CheckedGet(DIR_SRC_TEST_DATA_ROOT).value(), "/pkg");
}

#elif BUILDFLAG(IS_ANDROID)

// These keys are overridden in //base/test/test_support_android.cc.
TEST_F(PathServiceTest, AndroidTestOverrides) {
  EXPECT_EQ(PathService::CheckedGet(DIR_ANDROID_APP_DATA).value(),
            kExpectedChromiumTestsRoot);
  EXPECT_EQ(PathService::CheckedGet(DIR_ASSETS).value(),
            kExpectedChromiumTestsRoot);
  EXPECT_EQ(PathService::CheckedGet(DIR_SRC_TEST_DATA_ROOT).value(),
            kExpectedChromiumTestsRoot);
  EXPECT_EQ(PathService::CheckedGet(DIR_OUT_TEST_DATA_ROOT).value(),
            kExpectedChromiumTestsRoot);
}

#endif  // BUILDFLAG(IS_FUCHSIA)

}  // namespace base
