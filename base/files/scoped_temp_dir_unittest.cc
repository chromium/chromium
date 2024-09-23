// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include <shlobj.h>
#endif

namespace base {

TEST(ScopedTempDir, FullPath) {
  FilePath test_path;
  CreateNewTempDirectory(FILE_PATH_LITERAL("scoped_temp_dir"), &test_path);

  // Against an existing dir, it should get destroyed when leaving scope.
  EXPECT_TRUE(DirectoryExists(test_path));
  {
    ScopedTempDir dir;
    EXPECT_TRUE(dir.Set(test_path));
    EXPECT_TRUE(dir.IsValid());
  }
  EXPECT_FALSE(DirectoryExists(test_path));

  {
    ScopedTempDir dir;
    EXPECT_TRUE(dir.Set(test_path));
    // Now the dir doesn't exist, so ensure that it gets created.
    EXPECT_TRUE(DirectoryExists(test_path));
    // When we call Release(), it shouldn't get destroyed when leaving scope.
    FilePath path = dir.Take();
    EXPECT_EQ(path.value(), test_path.value());
    EXPECT_FALSE(dir.IsValid());
  }
  EXPECT_TRUE(DirectoryExists(test_path));

  // Clean up.
  {
    ScopedTempDir dir;
    EXPECT_TRUE(dir.Set(test_path));
  }
  EXPECT_FALSE(DirectoryExists(test_path));
}

TEST(ScopedTempDir, TempDir) {
  // In this case, just verify that a directory was created and that it's a
  // child of TempDir.
  FilePath test_path;
  {
    ScopedTempDir dir;
    EXPECT_TRUE(dir.CreateUniqueTempDir());
    test_path = dir.GetPath();
    EXPECT_TRUE(DirectoryExists(test_path));

#if BUILDFLAG(IS_WIN)
    FilePath expected_parent_dir;
    if (!::IsUserAnAdmin() ||
        !PathService::Get(DIR_SYSTEM_TEMP, &expected_parent_dir)) {
      EXPECT_TRUE(PathService::Get(DIR_TEMP, &expected_parent_dir));
    }
    EXPECT_TRUE(expected_parent_dir.IsParent(test_path));
#else   // BUILDFLAG(IS_WIN)
    FilePath tmp_dir;
    EXPECT_TRUE(GetTempDir(&tmp_dir));
    EXPECT_TRUE(test_path.value().find(tmp_dir.value()) != std::string::npos);
#endif  // BUILDFLAG(IS_WIN)
  }
  EXPECT_FALSE(DirectoryExists(test_path));
}

TEST(ScopedTempDir, UniqueTempDirUnderPath) {
  // Create a path which will contain a unique temp path.
  FilePath base_path;
  ASSERT_TRUE(
      CreateNewTempDirectory(FILE_PATH_LITERAL("base_dir"), &base_path));

  FilePath test_path;
  {
    ScopedTempDir dir;
    EXPECT_TRUE(dir.CreateUniqueTempDirUnderPath(base_path));
    test_path = dir.GetPath();
    EXPECT_TRUE(DirectoryExists(test_path));
    EXPECT_TRUE(base_path.IsParent(test_path));
    EXPECT_TRUE(test_path.value().find(base_path.value()) != std::string::npos);
  }
  EXPECT_FALSE(DirectoryExists(test_path));
  DeletePathRecursively(base_path);
}

TEST(ScopedTempDir, MultipleInvocations) {
  ScopedTempDir dir;
  EXPECT_TRUE(dir.CreateUniqueTempDir());
  EXPECT_FALSE(dir.CreateUniqueTempDir());
  EXPECT_TRUE(dir.Delete());
  EXPECT_TRUE(dir.CreateUniqueTempDir());
  EXPECT_FALSE(dir.CreateUniqueTempDir());
  ScopedTempDir other_dir;
  EXPECT_TRUE(other_dir.Set(dir.Take()));
  EXPECT_TRUE(dir.CreateUniqueTempDir());
  EXPECT_FALSE(dir.CreateUniqueTempDir());
  EXPECT_FALSE(other_dir.CreateUniqueTempDir());
}

TEST(ScopedTempDir, Move) {
  ScopedTempDir dir;
  EXPECT_TRUE(dir.CreateUniqueTempDir());
  FilePath dir_path = dir.GetPath();
  EXPECT_TRUE(DirectoryExists(dir_path));
  {
    ScopedTempDir other_dir(std::move(dir));
    EXPECT_EQ(dir_path, other_dir.GetPath());
    EXPECT_TRUE(DirectoryExists(dir_path));
    EXPECT_FALSE(dir.IsValid());
  }
  EXPECT_FALSE(DirectoryExists(dir_path));
}

#if BUILDFLAG(IS_WIN)
TEST(ScopedTempDir, LockedTempDir) {
  ScopedTempDir dir;
  EXPECT_TRUE(dir.CreateUniqueTempDir());
  File file(dir.GetPath().Append(FILE_PATH_LITERAL("temp")),
            File::FLAG_CREATE_ALWAYS | File::FLAG_WRITE);
  EXPECT_TRUE(file.IsValid());
  EXPECT_EQ(File::FILE_OK, file.error_details());
  EXPECT_FALSE(dir.Delete());  // We should not be able to delete.
  EXPECT_FALSE(dir.GetPath().empty());  // We should still have a valid path.
  file.Close();
  // Now, we should be able to delete.
  EXPECT_TRUE(dir.Delete());
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace base
