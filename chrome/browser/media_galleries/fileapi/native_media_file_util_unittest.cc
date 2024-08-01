// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/media_galleries/fileapi/native_media_file_util.h"

#include <stddef.h>

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/media_galleries/fileapi/media_file_system_backend.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "storage/browser/blob/shareable_file_reference.h"
#include "storage/browser/file_system/copy_or_move_hook_delegate.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_backend.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/file_system/isolated_context.h"
#include "storage/browser/file_system/native_file_util.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "storage/browser/test/test_file_system_options.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"
#include "url/origin.h"

#define FPL(x) FILE_PATH_LITERAL(x)

using storage::FileSystemOperation;
using storage::FileSystemURL;

namespace {

typedef FileSystemOperation::FileEntryList FileEntryList;

struct FilteringTestCase {
  const base::FilePath::CharType* path;
  bool is_directory;
  bool visible;
  bool media_file;
  const char* content;
};

const FilteringTestCase kFilteringTestCases[] = {
    // Directory should always be visible.
    {FPL("hoge"), true, true, false, nullptr},
    {FPL("fuga.jpg"), true, true, false, nullptr},
    {FPL("piyo.txt"), true, true, false, nullptr},
    {FPL("moga.cod"), true, true, false, nullptr},

    // File should be visible if it's a supported media file.
    // File without extension.
    {FPL("foo"), false, false, false, "abc"},
    // Supported media file.
    {FPL("bar.jpg"), false, true, true, "\xFF\xD8\xFF"},
    // Unsupported masquerading file.
    {FPL("sna.jpg"), false, true, false, "abc"},
    // Non-media file.
    {FPL("baz.txt"), false, false, false, "abc"},
    // Unsupported media file.
    {FPL("foobar.cod"), false, false, false, "abc"},
};

void ExpectEqHelper(const std::string& test_name,
                    base::File::Error expected,
                    base::File::Error actual) {
  EXPECT_EQ(expected, actual) << test_name;
}

void ExpectMetadataEqHelper(const std::string& test_name,
                            base::File::Error expected,
                            bool expected_is_directory,
                            base::File::Error actual,
                            const base::File::Info& file_info) {
  EXPECT_EQ(expected, actual) << test_name;
  if (actual == base::File::FILE_OK)
    EXPECT_EQ(expected_is_directory, file_info.is_directory) << test_name;
}

void DidReadDirectory(std::set<base::FilePath::StringType>* content,
                      bool* completed,
                      base::File::Error error,
                      FileEntryList file_list,
                      bool has_more) {
  EXPECT_TRUE(!*completed);
  *completed = !has_more;
  for (const auto& entry : file_list)
    EXPECT_TRUE(content->insert(entry.name.value()).second);
}

void PopulateDirectoryWithTestCases(const base::FilePath& dir,
                                    const FilteringTestCase* test_cases,
                                    size_t n) {
  for (size_t i = 0; i < n; ++i) {
    base::FilePath path = dir.Append(test_cases[i].path);
    if (test_cases[i].is_directory) {
      ASSERT_TRUE(base::CreateDirectory(path));
    } else {
      ASSERT_TRUE(test_cases[i].content);
      ASSERT_TRUE(base::WriteFile(path, test_cases[i].content));
    }
  }
}

}  // namespace

class NativeMediaFileUtilTest : public testing::Test {
 public:
  NativeMediaFileUtilTest() = default;

  NativeMediaFileUtilTest(const NativeMediaFileUtilTest&) = delete;
  NativeMediaFileUtilTest& operator=(const NativeMediaFileUtilTest&) = delete;

  void SetUp() override {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(base::CreateDirectory(root_path()));

    auto storage_policy =
        base::MakeRefCounted<storage::MockSpecialStoragePolicy>();

    std::vector<std::unique_ptr<storage::FileSystemBackend>>
        additional_providers;
    additional_providers.push_back(std::make_unique<MediaFileSystemBackend>(
        data_dir_.GetPath(), base::NullCallback()));

    file_system_context_ = storage::FileSystemContext::Create(
        content::GetIOThreadTaskRunner({}),
        base::SequencedTaskRunner::GetCurrentDefault(),
        storage::ExternalMountPoints::CreateRefCounted(),
        std::move(storage_policy),
        /* quota_manager_proxy=*/nullptr, std::move(additional_providers),
        std::vector<storage::URLRequestAutoMountHandler>(), data_dir_.GetPath(),
        storage::CreateAllowFileAccessOptions());

    filesystem_ = isolated_context()->RegisterFileSystemForPath(
        storage::kFileSystemTypeLocalMedia, std::string(), root_path(),
        nullptr);
    filesystem_id_ = filesystem_.id();
  }

  void TearDown() override { file_system_context_.reset(); }

 protected:
  storage::FileSystemContext* file_system_context() {
    return file_system_context_.get();
  }

  FileSystemURL CreateURL(const base::FilePath::CharType* test_case_path) {
    return file_system_context_->CreateCrackedFileSystemURL(
        blink::StorageKey::CreateFirstParty(url::Origin::Create(origin())),
        storage::kFileSystemTypeIsolated, GetVirtualPath(test_case_path));
  }

  storage::IsolatedContext* isolated_context() {
    return storage::IsolatedContext::GetInstance();
  }

  base::FilePath root_path() {
    return data_dir_.GetPath().Append(FPL("Media Directory"));
  }

  base::FilePath GetVirtualPath(
      const base::FilePath::CharType* test_case_path) {
    return base::FilePath::FromUTF8Unsafe(filesystem_id_)
        .Append(FPL("Media Directory"))
        .Append(base::FilePath(test_case_path));
  }

  GURL origin() { return GURL("http://example.com"); }

  storage::FileSystemType type() { return storage::kFileSystemTypeLocalMedia; }

  storage::FileSystemOperationRunner* operation_runner() {
    return file_system_context_->operation_runner();
  }

  content::BrowserTaskEnvironment task_environment_;

 private:
  base::ScopedTempDir data_dir_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;

  std::string filesystem_id_;
  storage::IsolatedContext::ScopedFSHandle filesystem_;
};

TEST_F(NativeMediaFileUtilTest, DirectoryExistsAndFileExistsFiltering) {
  PopulateDirectoryWithTestCases(root_path(), kFilteringTestCases,
                                 std::size(kFilteringTestCases));

  for (size_t i = 0; i < std::size(kFilteringTestCases); ++i) {
    FileSystemURL url = CreateURL(kFilteringTestCases[i].path);

    base::File::Error expectation = kFilteringTestCases[i].visible
                                        ? base::File::FILE_OK
                                        : base::File::FILE_ERROR_NOT_FOUND;

    std::string test_name =
        base::StringPrintf("DirectoryExistsAndFileExistsFiltering %" PRIuS, i);
    if (kFilteringTestCases[i].is_directory) {
      operation_runner()->DirectoryExists(
          url, base::BindOnce(&ExpectEqHelper, test_name, expectation));
    } else {
      operation_runner()->FileExists(
          url, base::BindOnce(&ExpectEqHelper, test_name, expectation));
    }
    content::RunAllTasksUntilIdle();
  }
}

TEST_F(NativeMediaFileUtilTest, ReadDirectoryFiltering) {
  PopulateDirectoryWithTestCases(root_path(), kFilteringTestCases,
                                 std::size(kFilteringTestCases));

  std::set<base::FilePath::StringType> content;
  FileSystemURL url = CreateURL(FPL(""));
  bool completed = false;
  operation_runner()->ReadDirectory(
      url, base::BindRepeating(&DidReadDirectory, &content, &completed));
  content::RunAllTasksUntilIdle();
  EXPECT_TRUE(completed);
  EXPECT_EQ(6u, content.size());

  for (size_t i = 0; i < std::size(kFilteringTestCases); ++i) {
    base::FilePath::StringType name =
        base::FilePath(kFilteringTestCases[i].path).BaseName().value();
    auto found = content.find(name);
    EXPECT_EQ(kFilteringTestCases[i].visible, found != content.end());
  }
}

TEST_F(NativeMediaFileUtilTest, CreateDirectoryFiltering) {
  // Run the loop twice. The second loop attempts to create directories that are
  // pre-existing. Though the result should be the same.
  for (int loop_count = 0; loop_count < 2; ++loop_count) {
    for (size_t i = 0; i < std::size(kFilteringTestCases); ++i) {
      if (kFilteringTestCases[i].is_directory) {
        FileSystemURL root_url = CreateURL(FPL(""));
        FileSystemURL url = CreateURL(kFilteringTestCases[i].path);

        std::string test_name = base::StringPrintf(
            "CreateFileAndCreateDirectoryFiltering run %d, test %" PRIuS,
            loop_count, i);
        base::File::Error expectation = kFilteringTestCases[i].visible
                                            ? base::File::FILE_OK
                                            : base::File::FILE_ERROR_SECURITY;
        operation_runner()->CreateDirectory(
            url, false, false,
            base::BindOnce(&ExpectEqHelper, test_name, expectation));
      }
      content::RunAllTasksUntilIdle();
    }
  }
}

TEST_F(NativeMediaFileUtilTest, CopySourceFiltering) {
  base::FilePath dest_path = root_path().AppendASCII("dest");
  FileSystemURL dest_url = CreateURL(FPL("dest"));

  // Run the loop twice. The first run has no source files. The second run does.
  for (int loop_count = 0; loop_count < 2; ++loop_count) {
    if (loop_count == 1) {
      PopulateDirectoryWithTestCases(root_path(), kFilteringTestCases,
                                     std::size(kFilteringTestCases));
    }
    for (size_t i = 0; i < std::size(kFilteringTestCases); ++i) {
      // Always start with an empty destination directory.
      // Copying to a non-empty destination directory is an invalid operation.
      ASSERT_TRUE(base::DeletePathRecursively(dest_path));
      ASSERT_TRUE(base::CreateDirectory(dest_path));

      FileSystemURL root_url = CreateURL(FPL(""));
      FileSystemURL url = CreateURL(kFilteringTestCases[i].path);

      std::string test_name = base::StringPrintf(
          "CopySourceFiltering run %d test %" PRIuS, loop_count, i);
      base::File::Error expectation = base::File::FILE_OK;
      if (loop_count == 0 || !kFilteringTestCases[i].visible) {
        // If the source does not exist or is not visible.
        expectation = base::File::FILE_ERROR_NOT_FOUND;
      } else if (!kFilteringTestCases[i].is_directory) {
        // Cannot copy a visible file to a directory.
        expectation = base::File::FILE_ERROR_INVALID_OPERATION;
      }
      operation_runner()->Copy(
          url, dest_url, storage::FileSystemOperation::CopyOrMoveOptionSet(),
          storage::FileSystemOperation::ERROR_BEHAVIOR_ABORT,
          std::make_unique<storage::CopyOrMoveHookDelegate>(),
          base::BindOnce(&ExpectEqHelper, test_name, expectation));
      content::RunAllTasksUntilIdle();
    }
  }
}

TEST_F(NativeMediaFileUtilTest, CopyDestFiltering) {
  // Run the loop twice. The first run has no destination files.
  // The second run does.
  for (int loop_count = 0; loop_count < 2; ++loop_count) {
    if (loop_count == 1) {
      // Reset the test directory between the two loops to remove old
      // directories and create new ones that should pre-exist.
      ASSERT_TRUE(base::DeletePathRecursively(root_path()));
      ASSERT_TRUE(base::CreateDirectory(root_path()));
      PopulateDirectoryWithTestCases(root_path(), kFilteringTestCases,
                                     std::size(kFilteringTestCases));
    }

    // Always create a dummy source data file.
    base::FilePath src_path = root_path().AppendASCII("foo.jpg");
    FileSystemURL src_url = CreateURL(FPL("foo.jpg"));
    static const char kDummyData[] = "dummy";
    ASSERT_TRUE(base::WriteFile(src_path, kDummyData));

    for (size_t i = 0; i < std::size(kFilteringTestCases); ++i) {
      if (loop_count == 0 && kFilteringTestCases[i].is_directory) {
        // These directories do not exist in this case, so Copy() will not
        // treat them as directories. Thus invalidating these test cases.
        continue;
      }
      FileSystemURL root_url = CreateURL(FPL(""));
      FileSystemURL url = CreateURL(kFilteringTestCases[i].path);

      std::string test_name = base::StringPrintf(
          "CopyDestFiltering run %d test %" PRIuS, loop_count, i);
      base::File::Error expectation;
      if (loop_count == 0) {
        // The destination path is a file here. The directory case has been
        // handled above.
        // If the destination path does not exist and is not visible, then
        // creating it would be a security violation.
        expectation = kFilteringTestCases[i].visible
                          ? base::File::FILE_OK
                          : base::File::FILE_ERROR_SECURITY;
      } else {
        if (!kFilteringTestCases[i].visible) {
          // If the destination path exist and is not visible, then to the copy
          // operation, it looks like the file needs to be created, which is a
          // security violation.
          expectation = base::File::FILE_ERROR_SECURITY;
        } else if (kFilteringTestCases[i].is_directory) {
          // Cannot copy a file to a directory.
          expectation = base::File::FILE_ERROR_INVALID_OPERATION;
        } else {
          // Copying from a file to a visible file that exists is ok.
          expectation = base::File::FILE_OK;
        }
      }
      operation_runner()->Copy(
          src_url, url, storage::FileSystemOperation::CopyOrMoveOptionSet(),
          storage::FileSystemOperation::ERROR_BEHAVIOR_ABORT,
          std::make_unique<storage::CopyOrMoveHookDelegate>(),
          base::BindOnce(&ExpectEqHelper, test_name, expectation));
      content::RunAllTasksUntilIdle();
    }
  }
}

TEST_F(NativeMediaFileUtilTest, MoveSourceFiltering) {
  base::FilePath dest_path = root_path().AppendASCII("dest");
  FileSystemURL dest_url = CreateURL(FPL("dest"));

  // Run the loop twice. The first run has no source files. The second run does.
  for (int loop_count = 0; loop_count < 2; ++loop_count) {
    if (loop_count == 1) {
      PopulateDirectoryWithTestCases(root_path(), kFilteringTestCases,
                                     std::size(kFilteringTestCases));
    }
    for (size_t i = 0; i < std::size(kFilteringTestCases); ++i) {
      // Always start with an empty destination directory.
      // Moving to a non-empty destination directory is an invalid operation.
      ASSERT_TRUE(base::DeletePathRecursively(dest_path));
      ASSERT_TRUE(base::CreateDirectory(dest_path));

      FileSystemURL root_url = CreateURL(FPL(""));
      FileSystemURL url = CreateURL(kFilteringTestCases[i].path);

      std::string test_name = base::StringPrintf(
          "MoveSourceFiltering run %d test %" PRIuS, loop_count, i);
      base::File::Error expectation = base::File::FILE_OK;
      if (loop_count == 0 || !kFilteringTestCases[i].visible) {
        // If the source does not exist or is not visible.
        expectation = base::File::FILE_ERROR_NOT_FOUND;
      } else if (!kFilteringTestCases[i].is_directory) {
        // Cannot move a visible file to a directory.
        expectation = base::File::FILE_ERROR_INVALID_OPERATION;
      }
      operation_runner()->Move(
          url, dest_url, storage::FileSystemOperation::CopyOrMoveOptionSet(),
          storage::FileSystemOperation::ERROR_BEHAVIOR_ABORT,
          std::make_unique<storage::CopyOrMoveHookDelegate>(),
          base::BindOnce(&ExpectEqHelper, test_name, expectation));
      content::RunAllTasksUntilIdle();
    }
  }
}

TEST_F(NativeMediaFileUtilTest, MoveDestFiltering) {
  // Run the loop twice. The first run has no destination files.
  // The second run does.
  for (int loop_count = 0; loop_count < 2; ++loop_count) {
    if (loop_count == 1) {
      // Reset the test directory between the two loops to remove old
      // directories and create new ones that should pre-exist.
      ASSERT_TRUE(base::DeletePathRecursively(root_path()));
      ASSERT_TRUE(base::CreateDirectory(root_path()));
      PopulateDirectoryWithTestCases(root_path(), kFilteringTestCases,
                                     std::size(kFilteringTestCases));
    }

    for (size_t i = 0; i < std::size(kFilteringTestCases); ++i) {
      if (loop_count == 0 && kFilteringTestCases[i].is_directory) {
        // These directories do not exist in this case, so Copy() will not
        // treat them as directories. Thus invalidating these test cases.
        continue;
      }

      // Create the source file for every test case because it might get moved.
      base::FilePath src_path = root_path().AppendASCII("foo.jpg");
      FileSystemURL src_url = CreateURL(FPL("foo.jpg"));
      static const char kDummyData[] = "dummy";
      ASSERT_TRUE(base::WriteFile(src_path, kDummyData));

      FileSystemURL root_url = CreateURL(FPL(""));
      FileSystemURL url = CreateURL(kFilteringTestCases[i].path);

      std::string test_name = base::StringPrintf(
          "MoveDestFiltering run %d test %" PRIuS, loop_count, i);
      base::File::Error expectation;
      if (loop_count == 0) {
        // The destination path is a file here. The directory case has been
        // handled above.
        // If the destination path does not exist and is not visible, then
        // creating it would be a security violation.
        expectation = kFilteringTestCases[i].visible
                          ? base::File::FILE_OK
                          : base::File::FILE_ERROR_SECURITY;
      } else {
        if (!kFilteringTestCases[i].visible) {
          // If the destination path exist and is not visible, then to the move
          // operation, it looks like the file needs to be created, which is a
          // security violation.
          expectation = base::File::FILE_ERROR_SECURITY;
        } else if (kFilteringTestCases[i].is_directory) {
          // Cannot move a file to a directory.
          expectation = base::File::FILE_ERROR_INVALID_OPERATION;
        } else {
          // Moving from a file to a visible file that exists is ok.
          expectation = base::File::FILE_OK;
        }
      }
      operation_runner()->Move(
          src_url, url, storage::FileSystemOperation::CopyOrMoveOptionSet(),
          storage::FileSystemOperation::ERROR_BEHAVIOR_ABORT,
          std::make_unique<storage::CopyOrMoveHookDelegate>(),
          base::BindOnce(&ExpectEqHelper, test_name, expectation));
      content::RunAllTasksUntilIdle();
    }
  }
}

TEST_F(NativeMediaFileUtilTest, GetMetadataFiltering) {
  // Run the loop twice. The first run has no files. The second run does.
  for (int loop_count = 0; loop_count < 2; ++loop_count) {
    if (loop_count == 1) {
      PopulateDirectoryWithTestCases(root_path(), kFilteringTestCases,
                                     std::size(kFilteringTestCases));
    }
    for (size_t i = 0; i < std::size(kFilteringTestCases); ++i) {
      FileSystemURL root_url = CreateURL(FPL(""));
      FileSystemURL url = CreateURL(kFilteringTestCases[i].path);

      std::string test_name = base::StringPrintf(
          "GetMetadataFiltering run %d test %" PRIuS, loop_count, i);
      base::File::Error expectation = base::File::FILE_OK;
      if (loop_count == 0 || !kFilteringTestCases[i].visible) {
        // Cannot get metadata from files that do not exist or are not visible.
        expectation = base::File::FILE_ERROR_NOT_FOUND;
      }
      operation_runner()->GetMetadata(
          url, {storage::FileSystemOperation::GetMetadataField::kIsDirectory},
          base::BindOnce(&ExpectMetadataEqHelper, test_name, expectation,
                         kFilteringTestCases[i].is_directory));
      content::RunAllTasksUntilIdle();
    }
  }
}

TEST_F(NativeMediaFileUtilTest, RemoveFileFiltering) {
  // Run the loop twice. The first run has no files. The second run does.
  for (int loop_count = 0; loop_count < 2; ++loop_count) {
    if (loop_count == 1) {
      PopulateDirectoryWithTestCases(root_path(), kFilteringTestCases,
                                     std::size(kFilteringTestCases));
    }
    for (size_t i = 0; i < std::size(kFilteringTestCases); ++i) {
      FileSystemURL root_url = CreateURL(FPL(""));
      FileSystemURL url = CreateURL(kFilteringTestCases[i].path);

      std::string test_name = base::StringPrintf(
          "RemoveFiltering run %d test %" PRIuS, loop_count, i);
      base::File::Error expectation = base::File::FILE_OK;
      if (loop_count == 0 || !kFilteringTestCases[i].visible) {
        // Cannot remove files that do not exist or are not visible.
        expectation = base::File::FILE_ERROR_NOT_FOUND;
      } else if (kFilteringTestCases[i].is_directory) {
        expectation = base::File::FILE_ERROR_NOT_A_FILE;
      }
      operation_runner()->RemoveFile(
          url, base::BindOnce(&ExpectEqHelper, test_name, expectation));
      content::RunAllTasksUntilIdle();
    }
  }
}

void CreateSnapshotCallback(base::File::Error* error,
                            base::File::Error result,
                            const base::File::Info&,
                            const base::FilePath&,
                            scoped_refptr<storage::ShareableFileReference>) {
  *error = result;
}

TEST_F(NativeMediaFileUtilTest, CreateSnapshot) {
  PopulateDirectoryWithTestCases(root_path(), kFilteringTestCases,
                                 std::size(kFilteringTestCases));
  for (size_t i = 0; i < std::size(kFilteringTestCases); ++i) {
    if (kFilteringTestCases[i].is_directory ||
        !kFilteringTestCases[i].visible) {
      continue;
    }
    FileSystemURL root_url = CreateURL(FPL(""));
    FileSystemURL url = CreateURL(kFilteringTestCases[i].path);
    base::File::Error expected_error, error;
    if (kFilteringTestCases[i].media_file)
      expected_error = base::File::FILE_OK;
    else
      expected_error = base::File::FILE_ERROR_SECURITY;
    error = base::File::FILE_ERROR_FAILED;
    operation_runner()->CreateSnapshotFile(
        url, base::BindOnce(CreateSnapshotCallback, &error));
    content::RunAllTasksUntilIdle();
    ASSERT_EQ(expected_error, error);
  }
}
