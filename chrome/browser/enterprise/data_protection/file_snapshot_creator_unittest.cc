// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_protection/file_snapshot_creator.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/file_system/file_system_backend.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/test_file_system_backend.h"
#include "storage/browser/test/test_file_system_context.h"
#include "storage/common/file_system/file_system_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_data_protection {

class FileSnapshotCreatorTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    std::vector<std::unique_ptr<storage::FileSystemBackend>>
        additional_providers;
    additional_providers.push_back(
        std::make_unique<storage::TestFileSystemBackend>(
            base::SingleThreadTaskRunner::GetCurrentDefault().get(),
            temp_dir_.GetPath()));

    file_system_context_ =
        storage::CreateFileSystemContextWithAdditionalProvidersForTesting(
            content::GetIOThreadTaskRunner({}),
            base::SingleThreadTaskRunner::GetCurrentDefault(),
            /*quota_manager_proxy=*/nullptr, std::move(additional_providers),
            temp_dir_.GetPath());
  }

  void TearDown() override {
    // Trigger asynchronous shutdown on the IO thread.
    file_system_context_->Shutdown();

    // Safely sequence the context destruction on the IO Thread and signal
    // completion back to the UI thread.
    base::RunLoop run_loop;
    content::GetIOThreadTaskRunner({})->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce([](scoped_refptr<storage::FileSystemContext> context) {},
                       std::move(file_system_context_)),
        run_loop.QuitClosure());

    run_loop.Run();
  }

  storage::FileSystemURL CreateVirtualFile(const std::string& filename,
                                           const std::string& content) {
    base::FilePath file_path = temp_dir_.GetPath().AppendASCII(filename);
    EXPECT_TRUE(base::WriteFile(file_path, content));

    const blink::StorageKey kTestStorageKey =
        blink::StorageKey::CreateFromStringForTesting("test-storage-key");
    return file_system_context_->CreateCrackedFileSystemURL(
        kTestStorageKey, storage::kFileSystemTypeTest,
        base::FilePath().AppendASCII(filename));
  }

  content::BrowserTaskEnvironment task_environment_{
      content::BrowserTaskEnvironment::REAL_IO_THREAD};
  base::ScopedTempDir temp_dir_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
};

TEST_F(FileSnapshotCreatorTest, SuccessfulSnapshotCreation) {
  std::string expected_content = "this is a file with non-empty content";
  storage::FileSystemURL url =
      CreateVirtualFile("test_file.txt", expected_content);

  base::test::TestFuture<const base::FilePath&> future;
  FileSnapshotCreator::Start(file_system_context_, url, future.GetCallback());

  base::FilePath snapshot_path = future.Get();
  ASSERT_FALSE(snapshot_path.empty());
  EXPECT_TRUE(base::PathExists(snapshot_path));

  std::string actual_content;
  EXPECT_TRUE(base::ReadFileToString(snapshot_path, &actual_content));
  EXPECT_EQ(expected_content, actual_content);

  // Clean up
  base::DeleteFile(snapshot_path);
}

TEST_F(FileSnapshotCreatorTest, EmptyFileSnapshotCreation) {
  std::string expected_content = "";
  storage::FileSystemURL url =
      CreateVirtualFile("empty_file.txt", expected_content);

  base::test::TestFuture<const base::FilePath&> future;
  FileSnapshotCreator::Start(file_system_context_, url, future.GetCallback());

  base::FilePath snapshot_path = future.Get();
  ASSERT_FALSE(snapshot_path.empty());
  EXPECT_TRUE(base::PathExists(snapshot_path));

  std::string actual_content;
  EXPECT_TRUE(base::ReadFileToString(snapshot_path, &actual_content));
  EXPECT_EQ(expected_content, actual_content);

  // Clean up
  base::DeleteFile(snapshot_path);
}

TEST_F(FileSnapshotCreatorTest, NonExistentFileErrorHandling) {
  const blink::StorageKey kTestStorageKey =
      blink::StorageKey::CreateFromStringForTesting("test-storage-key");
  storage::FileSystemURL url = file_system_context_->CreateCrackedFileSystemURL(
      kTestStorageKey, storage::kFileSystemTypeTest,
      base::FilePath().AppendASCII("non_existent_file.txt"));

  base::test::TestFuture<const base::FilePath&> future;
  FileSnapshotCreator::Start(file_system_context_, url, future.GetCallback());

  base::FilePath snapshot_path = future.Get();
  EXPECT_TRUE(snapshot_path.empty());
}

// TODO(crbug.com/519813376): Fails on ASan ChromeOS.
#if BUILDFLAG(IS_CHROMEOS) && defined(ADDRESS_SANITIZER)
#define MAYBE_MultiChunkFileCreation DISABLED_MultiChunkFileCreation
#else
#define MAYBE_MultiChunkFileCreation MultiChunkFileCreation
#endif
TEST_F(FileSnapshotCreatorTest, MAYBE_MultiChunkFileCreation) {
  std::string expected_content("a", FileSnapshotCreator::CHUNK_SIZE + 10);
  storage::FileSystemURL url =
      CreateVirtualFile("multi_chunk_file.txt", expected_content);

  base::test::TestFuture<const base::FilePath&> future;
  FileSnapshotCreator::Start(file_system_context_, url, future.GetCallback());

  base::FilePath snapshot_path = future.Get();
  ASSERT_FALSE(snapshot_path.empty());
  EXPECT_TRUE(base::PathExists(snapshot_path));

  std::string actual_content;
  EXPECT_TRUE(base::ReadFileToString(snapshot_path, &actual_content));
  EXPECT_EQ(expected_content, actual_content);

  // Clean up
  base::DeleteFile(snapshot_path);
}

}  // namespace enterprise_data_protection
