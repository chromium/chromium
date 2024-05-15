// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/fileapi/fallback_copy_in_foreign_file.h"

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/strcat.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/test/mock_quota_manager.h"
#include "storage/browser/test/mock_quota_manager_proxy.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "storage/browser/test/test_file_system_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

namespace ash {

class FallbackCopyInForeignFileTest : public testing::Test,
                                      public testing::WithParamInterface<bool> {
 public:
  static int CountFilesInDirectory(const base::FilePath& dir_path) {
    int count = 0;
    static constexpr bool recursive = false;
    base::FileEnumerator e(dir_path, recursive, base::FileEnumerator::FILES);
    while (!e.Next().empty()) {
      count++;
    }
    return count;
  }

  bool dest_file_already_exists() const { return GetParam(); }

  static std::string DescribeParams(
      const testing::TestParamInfo<ParamType>& info) {
    return info.param ? "DestFileAlreadyExists" : "DestFileDoesNotExist";
  }

 protected:
  void SetUp() override {
    ASSERT_TRUE(fs_context_temp_dir_.CreateUniqueTempDir());

    static constexpr bool is_incognito = false;
    scoped_refptr<storage::MockQuotaManager> quota_manager =
        base::MakeRefCounted<storage::MockQuotaManager>(
            is_incognito, fs_context_temp_dir_.GetPath(),
            base::SingleThreadTaskRunner::GetCurrentDefault(),
            base::MakeRefCounted<storage::MockSpecialStoragePolicy>());

    scoped_refptr<storage::MockQuotaManagerProxy> quota_manager_proxy =
        base::MakeRefCounted<storage::MockQuotaManagerProxy>(
            quota_manager.get(),
            base::SingleThreadTaskRunner::GetCurrentDefault());

    fs_context_ = CreateFileSystemContextForTesting(
        quota_manager_proxy, fs_context_temp_dir_.GetPath());
    ASSERT_NE(fs_context_.get(), nullptr);
  }

  base::test::TaskEnvironment task_environment_;

  base::ScopedTempDir fs_context_temp_dir_;
  scoped_refptr<storage::FileSystemContext> fs_context_;
};

TEST_P(FallbackCopyInForeignFileTest, Basic) {
  // Create a source temporary directory and a poem file within it.
  base::ScopedTempDir src_temp_dir;
  ASSERT_TRUE(src_temp_dir.CreateUniqueTempDir());
  base::FilePath src_file_path =
      src_temp_dir.GetPath().Append(FILE_PATH_LITERAL("src_poem.txt"));
  std::string poem("The frumious Bandersnatch!\n");
  ASSERT_TRUE(base::WriteFile(src_file_path, poem));

  // Create a destination temporary directory. If dest_file_already_exists()
  // then also create an existing file in that dest_temp_dir. It should be
  // overwritten (by being renamed and then deleted) by
  // FallbackCopyInForeignFile.
  base::ScopedTempDir dest_temp_dir;
  ASSERT_TRUE(dest_temp_dir.CreateUniqueTempDir());
  base::FilePath dest_file_path =
      dest_temp_dir.GetPath().Append(FILE_PATH_LITERAL("dest_poem.txt"));
  if (dest_file_already_exists()) {
    std::string occupied("The dest_file_path already exists.\n");
    ASSERT_TRUE(base::WriteFile(dest_file_path, occupied));
    ASSERT_EQ(CountFilesInDirectory(dest_temp_dir.GetPath()), 1);
  } else {
    ASSERT_EQ(CountFilesInDirectory(dest_temp_dir.GetPath()), 0);
  }

  // Register the mount point (and unregister at end-of-scope).
  static constexpr char mount_name[] = "FallbackCopyInForeignFileTest.Basic";
  EXPECT_TRUE(
      storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
          mount_name, storage::kFileSystemTypeLocal,
          storage::FileSystemMountOption(), dest_temp_dir.GetPath()));
  absl::Cleanup mount_points_unregisterer = [] {
    EXPECT_TRUE(
        storage::ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(
            mount_name));
  };

  // Call FallbackCopyInForeignFile.
  {
    storage::FileSystemURL dest_url = fs_context_->CrackURLInFirstPartyContext(
        GURL(base::StrCat({"filesystem:https://example.com/external/",
                           mount_name, "/dest_poem.txt"})));

    base::test::TestFuture<base::File::Error> future;

    FallbackCopyInForeignFile(
        *fs_context_->GetAsyncFileUtil(dest_url.type()),
        std::make_unique<storage::FileSystemOperationContext>(
            fs_context_.get()),
        src_file_path, dest_url, future.GetCallback());

    EXPECT_EQ(future.Get(), base::File::FILE_OK);
  }

  // Check the destination file's contents.
  {
    std::string dest_contents;
    base::ReadFileToString(dest_file_path, &dest_contents);
    EXPECT_EQ(poem, dest_contents);
  }

  // The destination temporary directory should contain exactly one file,
  // regardless of dest_file_already_exists().
  ASSERT_EQ(CountFilesInDirectory(dest_temp_dir.GetPath()), 1);
}

INSTANTIATE_TEST_SUITE_P(,
                         FallbackCopyInForeignFileTest,
                         testing::Bool(),
                         &FallbackCopyInForeignFileTest::DescribeParams);

}  // namespace ash
