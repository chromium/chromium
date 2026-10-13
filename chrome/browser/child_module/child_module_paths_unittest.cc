// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/child_module/child_module_paths.h"

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/test/scoped_path_override.h"
#include "build/build_config.h"
#include "chrome/common/child_module/child_module_helper.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include <optional>

#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/file_path_reparse_point_win.h"
#include "base/test/gmock_expected_support.h"
#include "base/win/access_token.h"
#include "base/win/sid.h"
#endif  // BUILDFLAG(IS_WIN)

namespace child_module {

#if BUILDFLAG(IS_WIN)

TEST(ChildModulePathsTest, InitializeChildModuleRoot) {
  base::ScopedPathOverride exe_dir_override(base::DIR_EXE);
  base::ScopedPathOverride user_data_dir_override(chrome::DIR_USER_DATA);
  base::ScopedPathOverride child_module_root_override(
      chrome::DIR_CHILD_MODULE_ROOT);
  const base::FilePath user_data_dir =
      base::PathService::CheckedGet(chrome::DIR_USER_DATA);
  ASSERT_OK_AND_ASSIGN(base::win::AccessToken token,
                       base::win::AccessToken::FromCurrentProcess());

  ASSERT_TRUE(InitializeChildModuleRoot());
  const base::FilePath child_module_root =
      base::PathService::CheckedGet(chrome::DIR_CHILD_MODULE_ROOT);
  EXPECT_EQ(child_module_root,
            base::PathService::CheckedGet(base::DIR_EXE)
                .AppendASCII(chrome::kChromeVersion)
                .Append(kModulesDirName)
                .Append(ComputeUserPathComponent(
                    token.User(), CanonicalizeUserDataDir(user_data_dir))));

  // A User Data directory that does not exist cannot be canonicalized.
  base::ScopedPathOverride nonexistent_override(
      chrome::DIR_USER_DATA, user_data_dir.AppendASCII("nonexistent"),
      /*is_absolute=*/true, /*create=*/false);
  EXPECT_FALSE(InitializeChildModuleRoot());
  EXPECT_EQ(base::PathService::CheckedGet(chrome::DIR_CHILD_MODULE_ROOT),
            child_module_root);
}

TEST(ChildModulePathsTest, CanonicalizeUserDataDir) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  const base::FilePath udd = temp_dir.GetPath().AppendASCII("test_udd");
  ASSERT_TRUE(base::CreateDirectory(udd));

  const base::FilePath canonical = CanonicalizeUserDataDir(udd);
  ASSERT_FALSE(canonical.empty());
  EXPECT_TRUE(canonical.IsAbsolute());
  EXPECT_EQ(canonical.BaseName(), base::FilePath(L"test_udd"));

  // Paths with "." components, different case, or a trailing separator all
  // produce the same result, with components in their on-disk case.
  EXPECT_EQ(CanonicalizeUserDataDir(
                temp_dir.GetPath().AppendASCII(".").AppendASCII("test_udd")),
            canonical);
  EXPECT_EQ(CanonicalizeUserDataDir(temp_dir.GetPath().AppendASCII("TEST_UDD")),
            canonical);
  EXPECT_EQ(CanonicalizeUserDataDir(udd.AsEndingWithSeparator()), canonical);

  // Junctions are resolved to their targets.
  const base::FilePath junction = temp_dir.GetPath().AppendASCII("junction");
  ASSERT_TRUE(base::CreateDirectory(junction));
  ASSERT_OK_AND_ASSIGN(auto reparse_point,
                       base::test::FilePathReparsePoint::Create(junction, udd));
  EXPECT_EQ(CanonicalizeUserDataDir(junction), canonical);

  // Nonexistent or empty paths cannot be canonicalized.
  EXPECT_EQ(
      CanonicalizeUserDataDir(temp_dir.GetPath().AppendASCII("nonexistent")),
      base::FilePath());
  EXPECT_EQ(CanonicalizeUserDataDir(base::FilePath()), base::FilePath());
}

#else  // BUILDFLAG(IS_WIN)

TEST(ChildModulePathsTest, InitializeChildModuleRoot) {
  base::ScopedPathOverride user_data_dir_override(chrome::DIR_USER_DATA);
  base::ScopedPathOverride child_module_root_override(
      chrome::DIR_CHILD_MODULE_ROOT);

  ASSERT_TRUE(InitializeChildModuleRoot());
  EXPECT_EQ(base::PathService::CheckedGet(chrome::DIR_CHILD_MODULE_ROOT),
            base::PathService::CheckedGet(chrome::DIR_USER_DATA)
                .Append(kModulesDirName)
                .AppendASCII(chrome::kChromeVersion));
}

#endif  // BUILDFLAG(IS_WIN)

}  // namespace child_module
