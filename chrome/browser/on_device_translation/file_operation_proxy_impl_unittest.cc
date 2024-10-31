// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/on_device_translation/file_operation_proxy_impl.h"

#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/files/memory_mapped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/services/on_device_translation/public/mojom/on_device_translation_service.mojom.h"
#include "components/services/on_device_translation/test/test_util.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/functions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace on_device_translation {

namespace {

// A FileOperationProxyImpl that runs a closure when it is deleted.
class FileOperationProxyImplWithDeletedClosure : public FileOperationProxyImpl {
 public:
  FileOperationProxyImplWithDeletedClosure(
      mojo::PendingReceiver<mojom::FileOperationProxy> proxy_receiver,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      std::vector<base::FilePath> package_paths,
      base::OnceClosure deleted_callback)
      : FileOperationProxyImpl(std::move(proxy_receiver),
                               std::move(task_runner),
                               std::move(package_paths)),
        closure_runner_(std::move(deleted_callback)) {}
  ~FileOperationProxyImplWithDeletedClosure() override = default;

 private:
  base::ScopedClosureRunner closure_runner_;
};

}  // namespace

// Unit tests for FileOperationProxyImpl.
class FileOperationProxyImplTest : public testing::Test {
 public:
  FileOperationProxyImplTest()
      : file_operation_proxy_(nullptr, base::OnTaskRunnerDeleter(nullptr)) {}
  ~FileOperationProxyImplTest() override = default;

  // testing::Test overrides.
  void TearDown() override {
    file_operation_proxy_.reset();
    // Wait for the background thread to finish deleting the proxy.
    if (run_loop_to_detect_proxy_deletion_) {
      run_loop_to_detect_proxy_deletion_->Run();
    }
  }

 protected:
  mojo::Remote<mojom::FileOperationProxy> CreateFileOperationProxy(
      std::vector<base::ScopedTempDir>&& package_dirs) {
    std::vector<base::FilePath> paths;
    for (const auto& path : package_dirs) {
      paths.emplace_back(path.GetPath());
    }

    mojo::Remote<mojom::FileOperationProxy> remote;
    // Create a task runner to run the FileOperationProxy.
    scoped_refptr<base::SequencedTaskRunner> task_runner =
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::USER_VISIBLE});
    // `run_loop_to_detect_proxy_deletion_` is used to wait for the background
    // thread to finish deleting the proxy to avoid a memory leak.
    CHECK(!run_loop_to_detect_proxy_deletion_);
    run_loop_to_detect_proxy_deletion_ = std::make_unique<base::RunLoop>();
    // Create the FileOperationProxy which lives in the background thread of
    // `task_runner`.
    file_operation_proxy_ =
        std::unique_ptr<FileOperationProxyImpl, base::OnTaskRunnerDeleter>(
            new FileOperationProxyImplWithDeletedClosure(
                remote.BindNewPipeAndPassReceiver(), task_runner,
                std::move(paths),
                run_loop_to_detect_proxy_deletion_->QuitClosure()),
            base::OnTaskRunnerDeleter(task_runner));
    return remote;
  }

  // Tests that the FileExists method returns the expected result.
  void TestFileExists(mojo::Remote<mojom::FileOperationProxy>& remote,
                      uint32_t package_index,
                      const std::string_view path_str,
                      bool expect_exists,
                      bool expect_is_directory) {
    base::RunLoop run_loop;
    remote->FileExists(
        package_index, base::FilePath::FromASCII(path_str),
        base::BindLambdaForTesting([&](bool exists, bool is_directory) {
          EXPECT_EQ(exists, expect_exists);
          EXPECT_EQ(is_directory, expect_is_directory);
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  // Tests that the FileExists method reports a bad message.
  void TestFileExistsReportBadMessage(
      mojo::Remote<mojom::FileOperationProxy>& remote,
      uint32_t package_index,
      const std::string_view path_str) {
    base::RunLoop run_loop;
    std::string received_error;
    mojo::SetDefaultProcessErrorHandler(
        base::BindLambdaForTesting([&](const std::string& error) {
          EXPECT_EQ(error, "Invalid `path` was passed.");
          run_loop.Quit();
        }));
    remote->FileExists(
        package_index, base::FilePath::FromASCII(path_str),
        base::BindLambdaForTesting(
            [](bool exists, bool is_directory) { NOTREACHED(); }));
    run_loop.Run();
  }

  // Tests that the Open method returns the expected file content.
  void TestOpen(mojo::Remote<mojom::FileOperationProxy>& remote,
                uint32_t package_index,
                const std::string_view path_str,
                const std::optional<std::string> expected_file_content) {
    base::RunLoop run_loop;
    base::File open_result;
    remote->Open(package_index, base::FilePath::FromASCII(path_str),
                 base::BindLambdaForTesting([&](base::File file) {
                   open_result = std::move(file);
                   run_loop.Quit();
                 }));
    run_loop.Run();
    if (!expected_file_content) {
      EXPECT_FALSE(open_result.IsValid());
      return;
    }
    base::MemoryMappedFile mapped_file;
    CHECK(mapped_file.Initialize(std::move(open_result)));
    EXPECT_THAT(
        std::string_view(reinterpret_cast<const char*>(mapped_file.data()),
                         mapped_file.length()),
        *expected_file_content);
  }

  // Tests that the Open method reports a bad message.
  void TestOpenReportBadMessage(mojo::Remote<mojom::FileOperationProxy>& remote,
                                uint32_t package_index,
                                const std::string_view path_str) {
    base::RunLoop run_loop;
    std::string received_error;
    mojo::SetDefaultProcessErrorHandler(
        base::BindLambdaForTesting([&](const std::string& error) {
          EXPECT_EQ(error, "Invalid `path` was passed.");
          run_loop.Quit();
        }));
    remote->Open(
        package_index, base::FilePath::FromASCII(path_str),
        base::BindLambdaForTesting([](base::File file) { NOTREACHED(); }));
    run_loop.Run();
  }

 private:
  std::vector<base::ScopedTempDir> package_paths_;
  std::unique_ptr<FileOperationProxyImpl, base::OnTaskRunnerDeleter>
      file_operation_proxy_;
  std::unique_ptr<base::RunLoop> run_loop_to_detect_proxy_deletion_;
  base::test::TaskEnvironment task_environment_;
};

// Tests of calling FileExists method for a file.
TEST_F(FileOperationProxyImplTest, FileExistsExistingFile) {
  std::vector<base::ScopedTempDir> dirs;
  dirs.emplace_back(SetupDataDir({{"filename", "test"}}));
  auto remote = CreateFileOperationProxy(std::move(dirs));
  TestFileExists(remote, 0u, "filename", /*expect_exists=*/true,
                 /*expect_is_dir*/ false);
}

// Tests of calling FileExists method for a directory.
TEST_F(FileOperationProxyImplTest, FileExistsExistingDirectory) {
  std::vector<base::ScopedTempDir> dirs;
  dirs.emplace_back(SetupDataDir({{"foo/bar", "test"}}));
  auto remote = CreateFileOperationProxy(std::move(dirs));
  TestFileExists(remote, 0u, "foo", /*expect_exists=*/true,
                 /*expect_is_dir*/ true);
}

// Tests of calling FileExists method for a file that does not exist.
TEST_F(FileOperationProxyImplTest, FileExistsNotExistingFile) {
  std::vector<base::ScopedTempDir> dirs;
  dirs.emplace_back(SetupDataDir({{"filename", "test"}}));
  auto remote = CreateFileOperationProxy(std::move(dirs));
  TestFileExists(remote, 0u, "not_exists", /*expect_exists=*/false,
                 /*expect_is_dir*/ false);
}

// Tests of calling FileExists method for multiple packages.
TEST_F(FileOperationProxyImplTest, FileExistsMultiplePackages) {
  std::vector<base::ScopedTempDir> dirs;
  dirs.emplace_back(
      SetupDataDir({{"filename1", "test1"}, {"dir1/file1", "data1"}}));
  dirs.emplace_back(
      SetupDataDir({{"filename2", "test2"}, {"dir2/file2", "data2"}}));
  auto remote = CreateFileOperationProxy(std::move(dirs));
  TestFileExists(remote, 0u, "filename1", /*expect_exists=*/true,
                 /*expect_is_dir*/ false);
  TestFileExists(remote, 0u, "dir1", /*expect_exists=*/true,
                 /*expect_is_dir*/ true);
  TestFileExists(remote, 0u, "dir1/file1", /*expect_exists=*/true,
                 /*expect_is_dir*/ false);

  TestFileExists(remote, 1u, "filename2", /*expect_exists=*/true,
                 /*expect_is_dir*/ false);
  TestFileExists(remote, 1u, "dir2", /*expect_exists=*/true,
                 /*expect_is_dir*/ true);
  TestFileExists(remote, 1u, "dir2/file2", /*expect_exists=*/true,
                 /*expect_is_dir*/ false);

  // Test for a file that does not exist.
  TestFileExists(remote, 0u, "filename2", /*expect_exists=*/false,
                 /*expect_is_dir*/ false);
  TestFileExists(remote, 0u, "dir2", /*expect_exists=*/false,
                 /*expect_is_dir*/ false);
  TestFileExists(remote, 0u, "dir2/file2", /*expect_exists=*/false,
                 /*expect_is_dir*/ false);
}

// Tests of calling FileExists method for an invalid package index.
TEST_F(FileOperationProxyImplTest, FileExistsInvalidPackageIndex) {
  std::vector<base::ScopedTempDir> dirs;
  dirs.emplace_back(SetupDataDir({{"filename", "test"}}));
  auto remote = CreateFileOperationProxy(std::move(dirs));
  TestFileExistsReportBadMessage(remote, 1u, "filename");
}

// Tests of calling FileExists method for an unexpected absolute path.
TEST_F(FileOperationProxyImplTest, FileExistsUnexpectedAbsolutePath) {
  std::vector<base::ScopedTempDir> dirs;
  dirs.emplace_back(SetupDataDir({{"filename", "test"}}));
  auto remote = CreateFileOperationProxy(std::move(dirs));
#if BUILDFLAG(IS_WIN)
  TestFileExistsReportBadMessage(remote, 0u, "X:\\filename");
#else
  TestFileExistsReportBadMessage(remote, 0u, "/filename");
#endif  // BUILDFLAG(IS_WIN)
}

// Tests of calling FileExists method for an unexpected path that references
// the parent directory.
TEST_F(FileOperationProxyImplTest, FileExistsUnexpectedReferencesParentPath) {
  std::vector<base::ScopedTempDir> dirs;
  dirs.emplace_back(SetupDataDir({{"filename", "test"}}));
  auto remote = CreateFileOperationProxy(std::move(dirs));
  TestFileExistsReportBadMessage(remote, 0u, "../filename");
}

// Tests of calling Open method for a file.
TEST_F(FileOperationProxyImplTest, OpenFile) {
  std::vector<base::ScopedTempDir> dirs;
  dirs.emplace_back(SetupDataDir({{"filename", "test"}}));
  auto remote = CreateFileOperationProxy(std::move(dirs));
  TestOpen(remote, 0u, "filename", "test");
}

// Tests of calling Open method for a file that does not exist.
TEST_F(FileOperationProxyImplTest, OpenNonExistingFile) {
  std::vector<base::ScopedTempDir> dirs;
  dirs.emplace_back(SetupDataDir({{"filename", "test"}}));
  auto remote = CreateFileOperationProxy(std::move(dirs));
  TestOpen(remote, 0u, "not_exists", std::nullopt);
}

// Tests of calling Open method for multiple packages.
TEST_F(FileOperationProxyImplTest, OpenMultiplePackages) {
  std::vector<base::ScopedTempDir> dirs;
  dirs.emplace_back(
      SetupDataDir({{"filename1", "test1"}, {"dir1/file1", "data1"}}));
  dirs.emplace_back(
      SetupDataDir({{"filename2", "test2"}, {"dir2/file2", "data2"}}));
  auto remote = CreateFileOperationProxy(std::move(dirs));
  TestOpen(remote, 0u, "filename1", "test1");
  TestOpen(remote, 0u, "dir1/file1", "data1");
  TestOpen(remote, 0u, "dir1", std::nullopt);

  TestOpen(remote, 1u, "filename2", "test2");
  TestOpen(remote, 1u, "dir2/file2", "data2");
  TestOpen(remote, 1u, "dir2", std::nullopt);

  // Test for a file that does not exist.
  TestOpen(remote, 0u, "filename2", std::nullopt);
  TestOpen(remote, 0u, "dir2", std::nullopt);
  TestOpen(remote, 0u, "dir2/file2", std::nullopt);
}

// Tests of calling Open method for an invalid package index.
TEST_F(FileOperationProxyImplTest, OpenInvalidPackageIndex) {
  std::vector<base::ScopedTempDir> dirs;
  dirs.emplace_back(SetupDataDir({{"filename", "test"}}));
  auto remote = CreateFileOperationProxy(std::move(dirs));
  TestOpenReportBadMessage(remote, 1u, "filename");
}

// Tests of calling Open method for an unexpected absolute path.
TEST_F(FileOperationProxyImplTest, OpenUnexpectedAbsolutePath) {
  std::vector<base::ScopedTempDir> dirs;
  dirs.emplace_back(SetupDataDir({{"filename", "test"}}));
  auto remote = CreateFileOperationProxy(std::move(dirs));
#if BUILDFLAG(IS_WIN)
  TestOpenReportBadMessage(remote, 0u, "X:\\filename");
#else
  TestOpenReportBadMessage(remote, 0u, "/filename");
#endif  // BUILDFLAG(IS_WIN)
}

// Tests of calling Open method for an unexpected path that references the
// parent directory.
TEST_F(FileOperationProxyImplTest, OpenUnexpectedReferencesParentPath) {
  std::vector<base::ScopedTempDir> dirs;
  dirs.emplace_back(SetupDataDir({{"filename", "test"}}));
  auto remote = CreateFileOperationProxy(std::move(dirs));
  TestOpenReportBadMessage(remote, 0u, "../filename");
}

}  // namespace on_device_translation
