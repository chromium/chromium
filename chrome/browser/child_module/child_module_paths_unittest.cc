// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/child_module/child_module_paths.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <optional>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/file_path_reparse_point_win.h"
#include "base/test/gmock_expected_support.h"
#include "testing/gtest/include/gtest/gtest.h"
#endif  // BUILDFLAG(IS_WIN)

namespace child_module {

#if BUILDFLAG(IS_WIN)
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
#endif  // BUILDFLAG(IS_WIN)

}  // namespace child_module
