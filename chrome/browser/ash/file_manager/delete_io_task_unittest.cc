// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/delete_io_task.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/files/dir_reader_posix.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/test_file_system_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

using ::base::test::RunClosure;
using ::testing::_;
using ::testing::AllOf;
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Return;
using ::testing::UnorderedElementsAre;

namespace file_manager {
namespace io_task {
namespace {

MATCHER_P(EntryStatusUrls, matcher, "") {
  std::vector<storage::FileSystemURL> urls;
  for (const auto& status : arg) {
    urls.push_back(status.url);
  }
  return testing::ExplainMatchResult(matcher, urls, result_listener);
}

MATCHER_P(EntryStatusErrors, matcher, "") {
  std::vector<std::optional<base::File::Error>> errors;
  for (const auto& status : arg) {
    errors.push_back(status.error);
  }
  return testing::ExplainMatchResult(matcher, errors, result_listener);
}

// Helper function that returns names found in the directory given by |path|.
std::vector<std::string> ReadDir(const std::string path) {
  base::DirReaderPosix reader(path.c_str());
  EXPECT_TRUE(reader.IsValid());
  std::vector<std::string> seen_names;
  while (reader.Next()) {
    seen_names.push_back(reader.name());
  }
  return seen_names;
}

class DeleteIOTaskTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    file_system_context_ = storage::CreateFileSystemContextForTesting(
        nullptr, temp_dir_.GetPath());
  }

  storage::FileSystemURL CreateFileSystemURL(const std::string& path) {
    return file_system_context_->CreateCrackedFileSystemURL(
        kTestStorageKey, storage::kFileSystemTypeTest,
        base::FilePath::FromUTF8Unsafe(path));
  }

  content::BrowserTaskEnvironment task_environment_;

  const blink::StorageKey kTestStorageKey =
      blink::StorageKey::CreateFromStringForTesting("chrome-extension://abc");

  base::ScopedTempDir temp_dir_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
};

TEST_F(DeleteIOTaskTest, Basic) {
  ASSERT_TRUE(base::WriteFile(temp_dir_.GetPath().Append("foo.txt"), "foo"));
  ASSERT_TRUE(base::WriteFile(temp_dir_.GetPath().Append("bar.txt"), "bar"));
  ASSERT_TRUE(base::WriteFile(temp_dir_.GetPath().Append("baz.txt"), "baz"));

  base::RunLoop run_loop;
  std::vector<storage::FileSystemURL> file_urls = {
      CreateFileSystemURL("foo.txt"),
      CreateFileSystemURL("bar.txt"),
  };
  auto base_matcher =
      AllOf(Field(&ProgressStatus::type, OperationType::kDelete),
            Field(&ProgressStatus::sources, EntryStatusUrls(file_urls)),
            Field(&ProgressStatus::total_bytes, 2));
  base::MockRepeatingCallback<void(const ProgressStatus&)> progress_callback;
  base::MockOnceCallback<void(ProgressStatus)> complete_callback;
  // We should get one progress callback when the first file is deleted.
  EXPECT_CALL(progress_callback,
              Run(AllOf(Field(&ProgressStatus::state, State::kInProgress),
                        Field(&ProgressStatus::bytes_transferred, 1),
                        Field(&ProgressStatus::sources,
                              EntryStatusErrors(ElementsAre(base::File::FILE_OK,
                                                            std::nullopt))),
                        base_matcher)))
      .Times(1);
  // We should get one complete callback when the both files are deleted.
  EXPECT_CALL(complete_callback,
              Run(AllOf(Field(&ProgressStatus::state, State::kSuccess),
                        Field(&ProgressStatus::bytes_transferred, 2),
                        Field(&ProgressStatus::sources,
                              EntryStatusErrors(ElementsAre(
                                  base::File::FILE_OK, base::File::FILE_OK))),
                        base_matcher)))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  DeleteIOTask task(file_urls, file_system_context_);
  task.Execute(progress_callback.Get(), complete_callback.Get());
  run_loop.Run();

  EXPECT_THAT(ReadDir(temp_dir_.GetPath().value()),
              UnorderedElementsAre(".", "..", "baz.txt"));
}

TEST_F(DeleteIOTaskTest, NoSuchFile) {
  base::RunLoop run_loop;

  std::vector<storage::FileSystemURL> file_urls = {
      CreateFileSystemURL("nonexistent_foo.txt"),
  };
  base::MockRepeatingCallback<void(const ProgressStatus&)> progress_callback;
  base::MockOnceCallback<void(ProgressStatus)> complete_callback;
  EXPECT_CALL(progress_callback, Run(_)).Times(0);
  // Note that deleting a file that does not exist is expected to succeed.
  // Whether the file is deleted or never created has the same end result.
  EXPECT_CALL(
      complete_callback,
      Run(AllOf(Field(&ProgressStatus::type, OperationType::kDelete),
                Field(&ProgressStatus::state, State::kSuccess),
                Field(&ProgressStatus::bytes_transferred, 1),
                Field(&ProgressStatus::sources, EntryStatusUrls(file_urls)),
                Field(&ProgressStatus::sources,
                      EntryStatusErrors(ElementsAre(base::File::FILE_OK))))))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  DeleteIOTask task(file_urls, file_system_context_);
  task.Execute(progress_callback.Get(), complete_callback.Get());
  run_loop.Run();
}

TEST_F(DeleteIOTaskTest, Cancel) {
  base::RunLoop run_loop;

  std::vector<storage::FileSystemURL> file_urls = {
      CreateFileSystemURL("foo.txt"),
      CreateFileSystemURL("bar.txt"),
  };
  base::MockRepeatingCallback<void(const ProgressStatus&)> progress_callback;
  base::MockOnceCallback<void(ProgressStatus)> complete_callback;
  EXPECT_CALL(progress_callback, Run(_)).Times(0);
  EXPECT_CALL(complete_callback, Run(_)).Times(0);
  {
    DeleteIOTask task(file_urls, file_system_context_);
    task.Execute(progress_callback.Get(), complete_callback.Get());
    task.Cancel();
    EXPECT_EQ(State::kCancelled, task.progress().state);
  }
  base::RunLoop().RunUntilIdle();
}

TEST_F(DeleteIOTaskTest, DeleteNothing) {
  base::RunLoop run_loop;

  std::vector<storage::FileSystemURL> file_urls;
  base::MockRepeatingCallback<void(const ProgressStatus&)> progress_callback;
  base::MockOnceCallback<void(ProgressStatus)> complete_callback;

  EXPECT_CALL(progress_callback, Run(_)).Times(0);
  EXPECT_CALL(
      complete_callback,
      Run(AllOf(
          Field(&ProgressStatus::type, OperationType::kDelete),
          Field(&ProgressStatus::state, State::kSuccess),
          Field(&ProgressStatus::bytes_transferred, 0),
          Field(&ProgressStatus::sources, EntryStatusUrls(file_urls)),
          Field(&ProgressStatus::sources, EntryStatusErrors(ElementsAre())))))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  DeleteIOTask task(file_urls, file_system_context_);
  task.Execute(progress_callback.Get(), complete_callback.Get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(DeleteIOTaskTest, DeleteFolder) {
  // Create a nested folders with files
  // tmp_dir
  //   |
  //   +-- baz.txt
  //   |
  //   +-- sub1
  //        |
  //        +-- foo.txt
  //        |
  //        +--- sub2
  //              |
  //              +-- bar.txt
  const base::FilePath tmp_path = temp_dir_.GetPath();
  const base::FilePath sub1_path = tmp_path.Append("sub");
  const base::FilePath sub2_path = sub1_path.Append("sub2");

  ASSERT_TRUE(base::WriteFile(tmp_path.Append("baz.txt"), "baz"));
  ASSERT_TRUE(base::CreateDirectory(sub1_path));
  ASSERT_TRUE(base::WriteFile(sub1_path.Append("foo.txt"), "foo"));
  ASSERT_TRUE(base::CreateDirectory(sub2_path));
  ASSERT_TRUE(base::WriteFile(sub2_path.Append("bar.txt"), "bar"));

  base::RunLoop run_loop;
  std::vector<storage::FileSystemURL> file_urls = {
      CreateFileSystemURL("sub"),
  };
  auto base_matcher =
      AllOf(Field(&ProgressStatus::type, OperationType::kDelete),
            Field(&ProgressStatus::sources, EntryStatusUrls(file_urls)),
            Field(&ProgressStatus::total_bytes, 1));
  base::MockRepeatingCallback<void(const ProgressStatus&)> progress_callback;
  base::MockOnceCallback<void(ProgressStatus)> complete_callback;
  // Since we are deleting just one element, expect no progress callbacks.
  EXPECT_CALL(progress_callback, Run(_)).Times(0);
  // We should get one complete callback when the the folder is deleted.
  EXPECT_CALL(
      complete_callback,
      Run(AllOf(Field(&ProgressStatus::state, State::kSuccess),
                Field(&ProgressStatus::bytes_transferred, 1),
                Field(&ProgressStatus::sources,
                      EntryStatusErrors(ElementsAre(base::File::FILE_OK))),
                base_matcher)))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  DeleteIOTask task(file_urls, file_system_context_);
  task.Execute(progress_callback.Get(), complete_callback.Get());
  run_loop.Run();

  EXPECT_THAT(ReadDir(temp_dir_.GetPath().value()),
              UnorderedElementsAre(".", "..", "baz.txt"));
}

}  // namespace
}  // namespace io_task
}  // namespace file_manager
