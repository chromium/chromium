// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/apple/backup_util.h"

#include <stddef.h>
#include <stdint.h>

#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/numerics/safe_conversions.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace base::apple {

namespace {

using BackupUtilTest = PlatformTest;

TEST_F(BackupUtilTest, TestExcludeFileFromBackups_Persists) {
  // The file must already exist in order to set its exclusion property.
  ScopedTempDir temp_dir_;
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  FilePath excluded_file_path = temp_dir_.GetPath().Append("excluded");
  constexpr char placeholder_data[] = "All your base are belong to us!";
  // Dump something real into the file.
  ASSERT_TRUE(WriteFile(excluded_file_path,
                        base::byte_span_from_cstring(placeholder_data)));
  // Initial state should be non-excluded.
  EXPECT_FALSE(GetBackupExclusion(excluded_file_path));
  // Exclude the file.
  ASSERT_TRUE(SetBackupExclusion(excluded_file_path));
  EXPECT_TRUE(GetBackupExclusion(excluded_file_path));
}

TEST_F(BackupUtilTest, TestExcludeFileFromBackups_NotByPath) {
  ScopedTempDir temp_dir_;
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  FilePath excluded_file_path = temp_dir_.GetPath().Append("excluded");
  ScopedCFTypeRef<CFURLRef> excluded_url =
      apple::FilePathToCFURL(excluded_file_path);

  constexpr char placeholder_data[] = "All your base are belong to us!";
  ASSERT_TRUE(WriteFile(excluded_file_path,
                        base::byte_span_from_cstring(placeholder_data)));

  ASSERT_TRUE(SetBackupExclusion(excluded_file_path));
  EXPECT_TRUE(GetBackupExclusion(excluded_file_path))
      << "Backup exclusion persists as long as the file exists";

  // Re-create the file.
  ASSERT_TRUE(DeleteFile(excluded_file_path));
  ASSERT_TRUE(WriteFile(excluded_file_path,
                        base::byte_span_from_cstring(placeholder_data)));
  EXPECT_FALSE(GetBackupExclusion(excluded_file_path))
      << "Re-created file should not be excluded from backup";
}

}  // namespace

}  // namespace base::apple
