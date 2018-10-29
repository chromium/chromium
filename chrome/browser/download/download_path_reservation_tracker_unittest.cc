// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/test_file_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/download/download_path_reservation_tracker.h"
#include "chrome/browser/download/download_target_determiner.h"
#include "components/download/public/common/mock_download_item.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "net/base/filename_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using download::DownloadItem;
using download::MockDownloadItem;
using testing::AnyNumber;
using testing::Return;
using testing::ReturnRef;
using testing::ReturnRefOfCopy;

namespace {

class DownloadPathReservationTrackerTest : public testing::Test {
 public:
  DownloadPathReservationTrackerTest();

  // testing::Test
  void SetUp() override;
  void TearDown() override;

  MockDownloadItem* CreateDownloadItem(int32_t id);
  base::FilePath GetPathInDownloadsDirectory(
      const base::FilePath::CharType* suffix);
  bool IsPathInUse(const base::FilePath& path);
  void CallGetReservedPath(
      DownloadItem* download_item,
      const base::FilePath& target_path,
      bool create_directory,
      DownloadPathReservationTracker::FilenameConflictAction conflict_action,
      base::FilePath* return_path,
      PathValidationResult* return_result);

  const base::FilePath& default_download_path() const {
    return default_download_path_;
  }
  void set_default_download_path(const base::FilePath& path) {
    default_download_path_ = path;
  }
  // Creates a name of form 'a'*repeat + suffix
  base::FilePath GetLongNamePathInDownloadsDirectory(
      size_t repeat,
      const base::FilePath::CharType* suffix);

 protected:
  base::ScopedTempDir test_download_dir_;
  base::FilePath default_download_path_;
  content::TestBrowserThreadBundle test_browser_thread_bundle_;

 private:
  void TestReservedPathCallback(base::FilePath* return_path,
                                PathValidationResult* return_result,
                                PathValidationResult result,
                                const base::FilePath& path);
};

DownloadPathReservationTrackerTest::DownloadPathReservationTrackerTest() =
    default;

void DownloadPathReservationTrackerTest::SetUp() {
  ASSERT_TRUE(test_download_dir_.CreateUniqueTempDir());
  set_default_download_path(test_download_dir_.GetPath());
}

void DownloadPathReservationTrackerTest::TearDown() {
  content::RunAllTasksUntilIdle();
}

MockDownloadItem* DownloadPathReservationTrackerTest::CreateDownloadItem(
    int32_t id) {
  MockDownloadItem* item = new ::testing::StrictMock<MockDownloadItem>;
  EXPECT_CALL(*item, GetId())
      .WillRepeatedly(Return(id));
  EXPECT_CALL(*item, GetTargetFilePath())
      .WillRepeatedly(ReturnRefOfCopy(base::FilePath()));
  EXPECT_CALL(*item, GetTemporaryFilePath())
      .WillRepeatedly(Return(base::FilePath()));
  EXPECT_CALL(*item, GetState())
      .WillRepeatedly(Return(DownloadItem::IN_PROGRESS));
  EXPECT_CALL(*item, GetURL()).WillRepeatedly(ReturnRefOfCopy(GURL()));
  EXPECT_CALL(*item, GetStartTime())
      .WillRepeatedly(Return(base::Time::UnixEpoch()));
  return item;
}

base::FilePath DownloadPathReservationTrackerTest::GetPathInDownloadsDirectory(
    const base::FilePath::CharType* suffix) {
  return default_download_path().Append(suffix).NormalizePathSeparators();
}

bool DownloadPathReservationTrackerTest::IsPathInUse(
    const base::FilePath& path) {
  content::RunAllTasksUntilIdle();
  return DownloadPathReservationTracker::IsPathInUseForTesting(path);
}

void DownloadPathReservationTrackerTest::CallGetReservedPath(
    DownloadItem* download_item,
    const base::FilePath& target_path,
    bool create_directory,
    DownloadPathReservationTracker::FilenameConflictAction conflict_action,
    base::FilePath* return_path,
    PathValidationResult* return_result) {
  // Weak pointer factory to prevent the callback from running after this
  // function has returned.
  base::WeakPtrFactory<DownloadPathReservationTrackerTest> weak_ptr_factory(
      this);
  DownloadPathReservationTracker::GetReservedPath(
      download_item, target_path, default_download_path(), create_directory,
      conflict_action,
      base::Bind(&DownloadPathReservationTrackerTest::TestReservedPathCallback,
                 weak_ptr_factory.GetWeakPtr(), return_path, return_result));
  content::RunAllTasksUntilIdle();
}

void DownloadPathReservationTrackerTest::TestReservedPathCallback(
    base::FilePath* return_path,
    PathValidationResult* return_result,
    PathValidationResult result,
    const base::FilePath& path) {
  *return_path = path;
  *return_result = result;
}

base::FilePath
DownloadPathReservationTrackerTest::GetLongNamePathInDownloadsDirectory(
    size_t repeat, const base::FilePath::CharType* suffix) {
  return GetPathInDownloadsDirectory(
      (base::FilePath::StringType(repeat, FILE_PATH_LITERAL('a'))
          + suffix).c_str());
}

void SetDownloadItemState(download::MockDownloadItem* download_item,
                          download::DownloadItem::DownloadState state) {
  EXPECT_CALL(*download_item, GetState())
      .WillRepeatedly(Return(state));
  download_item->NotifyObserversDownloadUpdated();
}

}  // namespace

// A basic reservation is acquired and committed.
TEST_F(DownloadPathReservationTrackerTest, BasicReservation) {
  std::unique_ptr<MockDownloadItem> item(CreateDownloadItem(1));
  base::FilePath path(
      GetPathInDownloadsDirectory(FILE_PATH_LITERAL("foo.txt")));
  ASSERT_FALSE(IsPathInUse(path));

  base::FilePath reserved_path;
  PathValidationResult result = PathValidationResult::NAME_TOO_LONG;
  DownloadPathReservationTracker::FilenameConflictAction conflict_action =
      DownloadPathReservationTracker::OVERWRITE;
  bool create_directory = false;
  CallGetReservedPath(item.get(), path, create_directory, conflict_action,
                      &reserved_path, &result);
  EXPECT_TRUE(IsPathInUse(path));
  EXPECT_EQ(PathValidationResult::SUCCESS, result);
  EXPECT_EQ(path.value(), reserved_path.value());

  // Destroying the item should release the reservation.
  SetDownloadItemState(item.get(), DownloadItem::COMPLETE);
  item.reset();
  content::RunAllTasksUntilIdle();
  EXPECT_FALSE(IsPathInUse(path));
}

// A download that is interrupted should lose its reservation.
TEST_F(DownloadPathReservationTrackerTest, InterruptedDownload) {
  std::unique_ptr<MockDownloadItem> item(CreateDownloadItem(1));
  base::FilePath path(
      GetPathInDownloadsDirectory(FILE_PATH_LITERAL("foo.txt")));
  ASSERT_FALSE(IsPathInUse(path));

  base::FilePath reserved_path;
  PathValidationResult result = PathValidationResult::NAME_TOO_LONG;
  DownloadPathReservationTracker::FilenameConflictAction conflict_action =
      DownloadPathReservationTracker::OVERWRITE;
  bool create_directory = false;
  CallGetReservedPath(item.get(), path, create_directory, conflict_action,
                      &reserved_path, &result);
  EXPECT_TRUE(IsPathInUse(path));
  EXPECT_EQ(PathValidationResult::SUCCESS, result);
  EXPECT_EQ(path.value(), reserved_path.value());

  // Once the download is interrupted, the path should become available again.
  SetDownloadItemState(item.get(), DownloadItem::INTERRUPTED);
  content::RunAllTasksUntilIdle();
  EXPECT_FALSE(IsPathInUse(path));
}

// A completed download should also lose its reservation.
TEST_F(DownloadPathReservationTrackerTest, CompleteDownload) {
  std::unique_ptr<MockDownloadItem> item(CreateDownloadItem(1));
  base::FilePath path(
      GetPathInDownloadsDirectory(FILE_PATH_LITERAL("foo.txt")));
  ASSERT_FALSE(IsPathInUse(path));

  base::FilePath reserved_path;
  PathValidationResult result = PathValidationResult::NAME_TOO_LONG;
  DownloadPathReservationTracker::FilenameConflictAction conflict_action =
      DownloadPathReservationTracker::OVERWRITE;
  bool create_directory = false;
  CallGetReservedPath(item.get(), path, create_directory, conflict_action,
                      &reserved_path, &result);
  EXPECT_TRUE(IsPathInUse(path));
  EXPECT_EQ(PathValidationResult::SUCCESS, result);
  EXPECT_EQ(path.value(), reserved_path.value());

  // Once the download completes, the path should become available again. For a
  // real download, at this point only the path reservation will be released.
  // The path wouldn't be available since it is occupied on disk by the
  // completed download.
  SetDownloadItemState(item.get(), DownloadItem::COMPLETE);
  content::RunAllTasksUntilIdle();
  EXPECT_FALSE(IsPathInUse(path));
}

// If there are files on the file system, a unique reservation should uniquify
// around it.
TEST_F(DownloadPathReservationTrackerTest, ConflictingFiles) {
  std::unique_ptr<MockDownloadItem> item(CreateDownloadItem(1));
  base::FilePath path(
      GetPathInDownloadsDirectory(FILE_PATH_LITERAL("foo.txt")));
  base::FilePath path1(
      GetPathInDownloadsDirectory(FILE_PATH_LITERAL("foo (1).txt")));
  // Create a file at |path|, and a .crdownload file at |path1|.
  ASSERT_EQ(0, base::WriteFile(path, "", 0));
  ASSERT_EQ(0,
            base::WriteFile(
                DownloadTargetDeterminer::GetCrDownloadPath(path1), "", 0));
  ASSERT_TRUE(IsPathInUse(path));

  base::FilePath reserved_path;
  PathValidationResult result = PathValidationResult::NAME_TOO_LONG;
  bool create_directory = false;
  DownloadPathReservationTracker::FilenameConflictAction conflict_action =
      DownloadPathReservationTracker::UNIQUIFY;
  CallGetReservedPath(item.get(), path, create_directory, conflict_action,
                      &reserved_path, &result);
  EXPECT_TRUE(IsPathInUse(path));
  EXPECT_TRUE(IsPathInUse(reserved_path));
  EXPECT_EQ(PathValidationResult::SUCCESS, result);
  // The path should be uniquified, skipping over foo.txt but not over
  // "foo (1).txt.crdownload"
  EXPECT_EQ(
      GetPathInDownloadsDirectory(FILE_PATH_LITERAL("foo (1).txt")).value(),
      reserved_path.value());

  SetDownloadItemState(item.get(), DownloadItem::COMPLETE);
  item.reset();
  content::RunAllTasksUntilIdle();
  EXPECT_TRUE(IsPathInUse(path));
  EXPECT_FALSE(IsPathInUse(reserved_path));
}

// If there are conflicting files on the file system, an overwriting reservation
// should succeed without altering the target path.
TEST_F(DownloadPathReservationTrackerTest, ConflictingFiles_Overwrite) {
  std::unique_ptr<MockDownloadItem> item(CreateDownloadItem(1));
  base::FilePath path(
      GetPathInDownloadsDirectory(FILE_PATH_LITERAL("foo.txt")));
  // Create a file at |path|.
  ASSERT_EQ(0, base::WriteFile(path, "", 0));
  ASSERT_TRUE(IsPathInUse(path));

  base::FilePath reserved_path;
  PathValidationResult result = PathValidationResult::NAME_TOO_LONG;
  bool create_directory = false;
  DownloadPathReservationTracker::FilenameConflictAction conflict_action =
      DownloadPathReservationTracker::OVERWRITE;
  CallGetReservedPath(item.get(), path, create_directory, conflict_action,
                      &reserved_path, &result);
  EXPECT_TRUE(IsPathInUse(path));
  EXPECT_TRUE(IsPathInUse(reserved_path));
  EXPECT_EQ(PathValidationResult::SUCCESS, result);
  EXPECT_EQ(path.value(), reserved_path.value());

  SetDownloadItemState(item.get(), DownloadItem::COMPLETE);
  item.reset();
  content::RunAllTasksUntilIdle();
}

// If the source is a file:// URL that is in the download directory, then Chrome
// could download the file onto itself. Test that this is flagged by DPRT.
TEST_F(DownloadPathReservationTrackerTest, ConflictWithSource) {
  std::unique_ptr<MockDownloadItem> item(CreateDownloadItem(1));
  base::FilePath path(
      GetPathInDownloadsDirectory(FILE_PATH_LITERAL("foo.txt")));
  ASSERT_EQ(0, base::WriteFile(path, "", 0));
  ASSERT_TRUE(IsPathInUse(path));
  EXPECT_CALL(*item, GetURL())
      .WillRepeatedly(ReturnRefOfCopy(net::FilePathToFileURL(path)));

  base::FilePath reserved_path;
  PathValidationResult result = PathValidationResult::NAME_TOO_LONG;
  bool create_directory = false;
  DownloadPathReservationTracker::FilenameConflictAction conflict_action =
      DownloadPathReservationTracker::UNIQUIFY;
  CallGetReservedPath(item.get(), path, create_directory, conflict_action,
                      &reserved_path, &result);
  EXPECT_EQ(PathValidationResult::SAME_AS_SOURCE, result);

  SetDownloadItemState(item.get(), DownloadItem::COMPLETE);
  item.reset();
  content::RunAllTasksUntilIdle();
}

// Multiple reservations for the same path should uniquify around each other.
TEST_F(DownloadPathReservationTrackerTest, ConflictingReservations) {
  std::unique_ptr<MockDownloadItem> item1(CreateDownloadItem(1));
  base::FilePath path(
      GetPathInDownloadsDirectory(FILE_PATH_LITERAL("foo.txt")));
  base::FilePath uniquified_path(
      GetPathInDownloadsDirectory(FILE_PATH_LITERAL("foo (1).txt")));
  ASSERT_FALSE(IsPathInUse(path));
  ASSERT_FALSE(IsPathInUse(uniquified_path));

  base::FilePath reserved_path1;
  PathValidationResult result = PathValidationResult::NAME_TOO_LONG;
  bool create_directory = false;

  DownloadPathReservationTracker::FilenameConflictAction conflict_action =
    DownloadPathReservationTracker::UNIQUIFY;
  CallGetReservedPath(item1.get(), path, create_directory, conflict_action,
                      &reserved_path1, &result);
  EXPECT_TRUE(IsPathInUse(path));
  EXPECT_EQ(PathValidationResult::SUCCESS, result);

  {
    // Requesting a reservation for the same path with uniquification results in
    // a uniquified path.
    std::unique_ptr<MockDownloadItem> item2(CreateDownloadItem(2));
    base::FilePath reserved_path2;
    CallGetReservedPath(item2.get(), path, create_directory, conflict_action,
                        &reserved_path2, &result);
    EXPECT_TRUE(IsPathInUse(path));
    EXPECT_TRUE(IsPathInUse(uniquified_path));
    EXPECT_EQ(uniquified_path.value(), reserved_path2.value());
    SetDownloadItemState(item2.get(), DownloadItem::COMPLETE);
  }
  content::RunAllTasksUntilIdle();
  EXPECT_TRUE(IsPathInUse(path));
  EXPECT_FALSE(IsPathInUse(uniquified_path));

  {
    // Since the previous download item was removed, requesting a reservation
    // for the same path should result in the same uniquified path.
    std::unique_ptr<MockDownloadItem> item2(CreateDownloadItem(2));
    base::FilePath reserved_path2;
    CallGetReservedPath(item2.get(), path, create_directory, conflict_action,
                        &reserved_path2, &result);
    EXPECT_TRUE(IsPathInUse(path));
    EXPECT_TRUE(IsPathInUse(uniquified_path));
    EXPECT_EQ(uniquified_path.value(), reserved_path2.value());
    SetDownloadItemState(item2.get(), DownloadItem::COMPLETE);
  }
  content::RunAllTasksUntilIdle();

  // Now acquire an overwriting reservation. We should end up with the same
  // non-uniquified path for both reservations.
  std::unique_ptr<MockDownloadItem> item3(CreateDownloadItem(2));
  base::FilePath reserved_path3;
  conflict_action = DownloadPathReservationTracker::OVERWRITE;
  CallGetReservedPath(item3.get(), path, create_directory, conflict_action,
                      &reserved_path3, &result);
  EXPECT_TRUE(IsPathInUse(path));
  EXPECT_FALSE(IsPathInUse(uniquified_path));

  EXPECT_EQ(path.value(), reserved_path1.value());
  EXPECT_EQ(path.value(), reserved_path3.value());

  SetDownloadItemState(item1.get(), DownloadItem::COMPLETE);
  SetDownloadItemState(item3.get(), DownloadItem::COMPLETE);
}

// Two active downloads shouldn't be able to reserve paths that only differ by
// case.
TEST_F(DownloadPathReservationTrackerTest, ConflictingCaseReservations) {
  std::unique_ptr<MockDownloadItem> item1(CreateDownloadItem(1));
  std::unique_ptr<MockDownloadItem> item2(CreateDownloadItem(2));

  base::FilePath path_foo =
      GetPathInDownloadsDirectory(FILE_PATH_LITERAL("foo.txt"));
  base::FilePath path_Foo =
      GetPathInDownloadsDirectory(FILE_PATH_LITERAL("Foo.txt"));

  base::FilePath first_reservation;
  PathValidationResult result = PathValidationResult::PATH_NOT_WRITABLE;
  CallGetReservedPath(item1.get(), path_foo, false,
                      DownloadPathReservationTracker::UNIQUIFY,
                      &first_reservation, &result);
  EXPECT_TRUE(IsPathInUse(path_foo));
  EXPECT_EQ(PathValidationResult::SUCCESS, result);
  EXPECT_EQ(path_foo, first_reservation);

  // Foo should also be in use at this point.
  EXPECT_TRUE(IsPathInUse(path_Foo));

  base::FilePath second_reservation;
  CallGetReservedPath(item2.get(), path_Foo, false,
                      DownloadPathReservationTracker::UNIQUIFY,
                      &second_reservation, &result);
  EXPECT_EQ(PathValidationResult::SUCCESS, result);
  EXPECT_EQ(GetPathInDownloadsDirectory(FILE_PATH_LITERAL("Foo (1).txt")),
            second_reservation);
  SetDownloadItemState(item1.get(), DownloadItem::COMPLETE);
  SetDownloadItemState(item2.get(), DownloadItem::COMPLETE);
}

// If a unique path cannot be determined after trying kMaxUniqueFiles
// uniquifiers, then the callback should notified that verification failed, and
// the returned path should be set to the original requested path.
TEST_F(DownloadPathReservationTrackerTest, UnresolvedConflicts) {
  base::FilePath path(
      GetPathInDownloadsDirectory(FILE_PATH_LITERAL("foo.txt")));
  // Make room for the path with no uniquifier, the |kMaxUniqueFiles|
  // numerically uniquified paths, and then one more for the timestamp
  // uniquified path.
  std::unique_ptr<MockDownloadItem>
      items[DownloadPathReservationTracker::kMaxUniqueFiles + 2];
  DownloadPathReservationTracker::FilenameConflictAction conflict_action =
    DownloadPathReservationTracker::UNIQUIFY;
  bool create_directory = false;

  // Create |kMaxUniqueFiles + 2| reservations for |path|. The first reservation
  // will have no uniquifier. Then |kMaxUniqueFiles| paths have numeric
  // uniquifiers. Then one more will have a timestamp uniquifier.
  for (int i = 0; i <= DownloadPathReservationTracker::kMaxUniqueFiles + 1;
       i++) {
    SCOPED_TRACE(testing::Message() << "i = " << i);
    base::FilePath reserved_path;
    base::FilePath expected_path;
    PathValidationResult result = PathValidationResult::NAME_TOO_LONG;
    if (i == 0) {
      expected_path = path;
    } else if (i > 0 && i <= DownloadPathReservationTracker::kMaxUniqueFiles) {
      expected_path =
          path.InsertBeforeExtensionASCII(base::StringPrintf(" (%d)", i));
    } else {
      expected_path =
          path.InsertBeforeExtensionASCII(" - 1970-01-01T00:00:00.000Z");
    }
    items[i].reset(CreateDownloadItem(i));
    EXPECT_FALSE(IsPathInUse(expected_path));
    CallGetReservedPath(items[i].get(), path, create_directory, conflict_action,
                        &reserved_path, &result);
    EXPECT_TRUE(IsPathInUse(expected_path));
    EXPECT_EQ(expected_path.value(), reserved_path.value());
    EXPECT_EQ(PathValidationResult::SUCCESS, result);
  }
  // The next reservation for |path| will fail to be unique.
  std::unique_ptr<MockDownloadItem> item(
      CreateDownloadItem(DownloadPathReservationTracker::kMaxUniqueFiles + 2));
  base::FilePath reserved_path;
  PathValidationResult result = PathValidationResult::NAME_TOO_LONG;
  CallGetReservedPath(item.get(), path, create_directory, conflict_action,
                      &reserved_path, &result);
  EXPECT_EQ(PathValidationResult::CONFLICT, result);
  EXPECT_EQ(path.value(), reserved_path.value());

  SetDownloadItemState(item.get(), DownloadItem::COMPLETE);
  for (auto& item : items)
    SetDownloadItemState(item.get(), DownloadItem::COMPLETE);
}

// If the target directory is unwriteable, then callback should be notified that
// verification failed.
TEST_F(DownloadPathReservationTrackerTest, UnwriteableDirectory) {
  std::unique_ptr<MockDownloadItem> item(CreateDownloadItem(1));
  base::FilePath path(
      GetPathInDownloadsDirectory(FILE_PATH_LITERAL("foo.txt")));
  base::FilePath dir(path.DirName());
  ASSERT_FALSE(IsPathInUse(path));

  {
    // Scope for FilePermissionRestorer
    base::FilePermissionRestorer restorer(dir);
    EXPECT_TRUE(base::MakeFileUnwritable(dir));
    base::FilePath reserved_path;
    PathValidationResult result = PathValidationResult::NAME_TOO_LONG;
    DownloadPathReservationTracker::FilenameConflictAction conflict_action =
        DownloadPathReservationTracker::OVERWRITE;
    bool create_directory = false;
    CallGetReservedPath(item.get(), path, create_directory, conflict_action,
                        &reserved_path, &result);
    // Verification fails.
    EXPECT_EQ(PathValidationResult::PATH_NOT_WRITABLE, result);
    EXPECT_EQ(path.BaseName().value(), reserved_path.BaseName().value());
  }
  SetDownloadItemState(item.get(), DownloadItem::COMPLETE);
}

// If the default download directory doesn't exist, then it should be
// created. But only if we are actually going to create the download path there.
TEST_F(DownloadPathReservationTrackerTest, CreateDefaultDownloadPath) {
  base::FilePath path(
      GetPathInDownloadsDirectory(FILE_PATH_LITERAL("foo/foo.txt")));
  base::FilePath dir(path.DirName());
  ASSERT_FALSE(base::DirectoryExists(dir));
  DownloadPathReservationTracker::FilenameConflictAction conflict_action =
    DownloadPathReservationTracker::OVERWRITE;
  bool create_directory = false;

  {
    std::unique_ptr<MockDownloadItem> item(CreateDownloadItem(1));
    base::FilePath reserved_path;
    PathValidationResult result = PathValidationResult::NAME_TOO_LONG;
    CallGetReservedPath(item.get(), path, create_directory, conflict_action,
                        &reserved_path, &result);
    // Verification fails because the directory doesn't exist.
    EXPECT_EQ(PathValidationResult::PATH_NOT_WRITABLE, result);
    SetDownloadItemState(item.get(), DownloadItem::COMPLETE);
  }
  ASSERT_FALSE(IsPathInUse(path));
  {
    std::unique_ptr<MockDownloadItem> item(CreateDownloadItem(1));
    base::FilePath reserved_path;
    PathValidationResult result = PathValidationResult::NAME_TOO_LONG;
    set_default_download_path(dir);
    CallGetReservedPath(item.get(), path, create_directory, conflict_action,
                        &reserved_path, &result);
    // Verification succeeds because the directory is created.
    EXPECT_EQ(PathValidationResult::SUCCESS, result);
    EXPECT_TRUE(base::DirectoryExists(dir));
    SetDownloadItemState(item.get(), DownloadItem::COMPLETE);
  }
}

// If the target path of the download item changes, the reservation should be
// updated to match.
TEST_F(DownloadPathReservationTrackerTest, UpdatesToTargetPath) {
  std::unique_ptr<MockDownloadItem> item(CreateDownloadItem(1));
  base::FilePath path(
      GetPathInDownloadsDirectory(FILE_PATH_LITERAL("foo.txt")));
  ASSERT_FALSE(IsPathInUse(path));

  base::FilePath reserved_path;
  PathValidationResult result = PathValidationResult::NAME_TOO_LONG;
  DownloadPathReservationTracker::FilenameConflictAction conflict_action =
      DownloadPathReservationTracker::OVERWRITE;
  bool create_directory = false;
  CallGetReservedPath(item.get(), path, create_directory, conflict_action,
                      &reserved_path, &result);
  EXPECT_TRUE(IsPathInUse(path));
  EXPECT_EQ(PathValidationResult::SUCCESS, result);
  EXPECT_EQ(path.value(), reserved_path.value());

  // The target path is initially empty. If an OnDownloadUpdated() is issued in
  // this state, we shouldn't lose the reservation.
  ASSERT_EQ(base::FilePath::StringType(), item->GetTargetFilePath().value());
  item->NotifyObserversDownloadUpdated();
  content::RunAllTasksUntilIdle();
  EXPECT_TRUE(IsPathInUse(path));

  // If the target path changes, we should update the reservation to match.
  base::FilePath new_target_path(
      GetPathInDownloadsDirectory(FILE_PATH_LITERAL("bar.txt")));
  ASSERT_FALSE(IsPathInUse(new_target_path));
  EXPECT_CALL(*item, GetTargetFilePath())
      .WillRepeatedly(ReturnRef(new_target_path));
  item->NotifyObserversDownloadUpdated();
  content::RunAllTasksUntilIdle();
  EXPECT_FALSE(IsPathInUse(path));
  EXPECT_TRUE(IsPathInUse(new_target_path));

  // Destroying the item should release the reservation.
  SetDownloadItemState(item.get(), DownloadItem::COMPLETE);
  item.reset();
  content::RunAllTasksUntilIdle();
  EXPECT_FALSE(IsPathInUse(new_target_path));
}

// Tests for long name truncation. On other platforms automatic truncation
// is not performed (yet).
#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS)

TEST_F(DownloadPathReservationTrackerTest, BasicTruncation) {
  int real_max_length =
      base::GetMaximumPathComponentLength(default_download_path());
  ASSERT_NE(-1, real_max_length);

#if defined(OS_WIN)
  const size_t max_length = real_max_length - strlen(":Zone.Identifier");
#else
  // TODO(kinaba): the current implementation leaves spaces for appending
  // ".crdownload". So take it into account. Should be removed in the future.
  const size_t max_length = real_max_length - 11;
#endif  // defined(OS_WIN)

  std::unique_ptr<MockDownloadItem> item(CreateDownloadItem(1));
  base::FilePath path(GetLongNamePathInDownloadsDirectory(
      max_length, FILE_PATH_LITERAL(".txt")));
  ASSERT_FALSE(IsPathInUse(path));

  base::FilePath reserved_path;
  PathValidationResult result = PathValidationResult::NAME_TOO_LONG;
  DownloadPathReservationTracker::FilenameConflictAction conflict_action =
      DownloadPathReservationTracker::OVERWRITE;
  bool create_directory = false;
  CallGetReservedPath(item.get(), path, create_directory, conflict_action,
                      &reserved_path, &result);
  EXPECT_TRUE(IsPathInUse(reserved_path));
  EXPECT_EQ(PathValidationResult::SUCCESS, result);
  // The file name length is truncated to max_length.
  EXPECT_EQ(max_length, reserved_path.BaseName().value().size());
  // But the extension is kept unchanged.
  EXPECT_EQ(path.Extension(), reserved_path.Extension());
  SetDownloadItemState(item.get(), DownloadItem::COMPLETE);
}

TEST_F(DownloadPathReservationTrackerTest, TruncationConflict) {
  int real_max_length =
      base::GetMaximumPathComponentLength(default_download_path());
  ASSERT_NE(-1, real_max_length);
#if defined(OS_WIN)
  const size_t max_length = real_max_length - strlen(":Zone.Identifier");
#else
  const size_t max_length = real_max_length - 11;
#endif  // defined(OS_WIN)

  std::unique_ptr<MockDownloadItem> item(CreateDownloadItem(1));
  base::FilePath path(GetLongNamePathInDownloadsDirectory(
      max_length, FILE_PATH_LITERAL(".txt")));
  base::FilePath path0(GetLongNamePathInDownloadsDirectory(
      max_length - 4, FILE_PATH_LITERAL(".txt")));
  base::FilePath path1(GetLongNamePathInDownloadsDirectory(
      max_length - 8, FILE_PATH_LITERAL(" (1).txt")));
  base::FilePath path2(GetLongNamePathInDownloadsDirectory(
      max_length - 8, FILE_PATH_LITERAL(" (2).txt")));
  ASSERT_FALSE(IsPathInUse(path));
  // "aaa...aaaaaaa.txt" (truncated path) and
  // "aaa...aaa (1).txt" (truncated and first uniquification try) exists.
  // "aaa...aaa (2).txt" should be used.
  ASSERT_EQ(0, base::WriteFile(path0, "", 0));
  ASSERT_EQ(0, base::WriteFile(path1, "", 0));

  base::FilePath reserved_path;
  PathValidationResult result = PathValidationResult::NAME_TOO_LONG;
  DownloadPathReservationTracker::FilenameConflictAction conflict_action =
    DownloadPathReservationTracker::UNIQUIFY;
  bool create_directory = false;
  CallGetReservedPath(item.get(), path, create_directory, conflict_action,
                      &reserved_path, &result);
  EXPECT_TRUE(IsPathInUse(reserved_path));
  EXPECT_EQ(PathValidationResult::SUCCESS, result);
  EXPECT_EQ(path2, reserved_path);
  SetDownloadItemState(item.get(), DownloadItem::COMPLETE);
}

TEST_F(DownloadPathReservationTrackerTest, TruncationFail) {
  int real_max_length =
      base::GetMaximumPathComponentLength(default_download_path());
  ASSERT_NE(-1, real_max_length);
#if defined(OS_WIN)
  const size_t max_length = real_max_length - strlen(":Zone.Identifier");
#else
  const size_t max_length = real_max_length - 11;
#endif  // defined(OS_WIN)

  std::unique_ptr<MockDownloadItem> item(CreateDownloadItem(1));
  base::FilePath path(GetPathInDownloadsDirectory(
      (FILE_PATH_LITERAL("a.") +
          base::FilePath::StringType(max_length, 'b')).c_str()));
  ASSERT_FALSE(IsPathInUse(path));

  base::FilePath reserved_path;
  PathValidationResult result = PathValidationResult::SUCCESS;
  DownloadPathReservationTracker::FilenameConflictAction conflict_action =
      DownloadPathReservationTracker::OVERWRITE;
  bool create_directory = false;
  CallGetReservedPath(item.get(), path, create_directory, conflict_action,
                      &reserved_path, &result);
  // We cannot truncate a path with very long extension.
  EXPECT_EQ(PathValidationResult::NAME_TOO_LONG, result);
  SetDownloadItemState(item.get(), DownloadItem::COMPLETE);
}

#endif  // Platforms that support filename truncation.
