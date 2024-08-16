// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/arc/file_system_watcher/file_system_scanner.h"

#include <unistd.h>

#include <string>
#include <vector>

#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "ash/components/arc/test/fake_file_system_instance.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/time/time.h"
#include "chrome/browser/ash/arc/file_system_watcher/arc_file_system_watcher_util.h"
#include "chrome/browser/ash/arc/fileapi/arc_file_system_bridge.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Return;

namespace arc {

namespace {

constexpr char kTestingProfileName[] = "test-user";

class FileUtil {
 public:
  void SetLastChangeTime(const base::FilePath& path, base::Time ctime) {
    ctimes_[path] = ctime;
  }

  base::Time GetLastChangeTime(const base::FilePath& path) {
    return ctimes_[path];
  }

 private:
  std::map<base::FilePath, base::Time> ctimes_;
};

}  // namespace

class ArcFileSystemScannerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    android_dir_ = base::FilePath(FakeFileSystemInstance::kFakeAndroidPath);
    // TODO(risan): ASSERT_TRUE inside this won't terminate. Instead, this
    // should return boolean and we should return from this SetUp() when it is
    // false.
    CreateDummyFilesAndDirectories();

    // Setting up profile.
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    Profile* profile =
        profile_manager_->CreateTestingProfile(kTestingProfileName);

    // Setting up FakeFileSystemInstance.
    auto ctime_callback = base::BindRepeating(&FileUtil::GetLastChangeTime,
                                              base::Unretained(&file_util_));
    file_system_instance_.SetGetLastChangeTimeCallback(ctime_callback);
    file_system_instance_.SetCrosDir(temp_dir_.GetPath());

    // Setting up ArcBridgeService and inject FakeFileSystemInstance.
    arc_file_system_bridge_ =
        std::make_unique<ArcFileSystemBridge>(profile, &arc_bridge_service_);
    arc_bridge_service_.file_system()->SetInstance(&file_system_instance_);
    WaitForInstanceReady(arc_bridge_service_.file_system());
    ASSERT_TRUE(file_system_instance_.InitCalled());

    // Setting up the FileSystemScanner.
    file_system_scanner_ = std::make_unique<FileSystemScanner>(
        temp_dir_.GetPath(), android_dir_, &arc_bridge_service_,
        ctime_callback);
  }

  void TearDown() override {
    arc_bridge_service_.file_system()->CloseInstance(&file_system_instance_);
    arc_file_system_bridge_.reset();
    profile_manager_.reset();
    expected_media_store_.clear();
  }

  void CreateDummyFilesAndDirectories() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    base::FilePath dir = temp_dir_.GetPath();
    ASSERT_TRUE(CreateDirectory(dir, base::Time()));
    ASSERT_TRUE(CreateFile(dir.AppendASCII("1.png"), base::Time()));
    ASSERT_TRUE(CreateDirectory(dir.AppendASCII("a"), base::Time()));
    ASSERT_TRUE(CreateFile(dir.AppendASCII("a/2.png"), base::Time()));
    ASSERT_TRUE(CreateDirectory(dir.AppendASCII("a/b"), base::Time()));
    ASSERT_TRUE(CreateFile(dir.AppendASCII("a/b/3.png"), base::Time()));
  }

  // TODO(risan): expected_media_store_ needs to be set by the callers
  // instead of here. It should be called by the callers for explicitness.
  void ModifyDirectory(const base::FilePath& path, base::Time ctime) {
    expected_media_store_[GetAndroidPath(path, temp_dir_.GetPath(),
                                         android_dir_)] = base::Time();
    file_util_.SetLastChangeTime(path, ctime);
  }

  void ModifyFile(const base::FilePath& path, base::Time ctime) {
    expected_media_store_[GetAndroidPath(path, temp_dir_.GetPath(),
                                         android_dir_)] = ctime;
    file_util_.SetLastChangeTime(path, ctime);
  }

  bool CreateFile(const base::FilePath& path, base::Time ctime) {
    base::FilePath parent = path.DirName();
    ModifyFile(path, ctime);
    ModifyDirectory(parent, ctime);
    return WriteFile(path, "42");
  }

  bool CreateDirectory(const base::FilePath& path, base::Time ctime) {
    file_util_.SetLastChangeTime(path, ctime);
    ModifyDirectory(path.DirName(), ctime);
    return base::CreateDirectory(path);
  }

  bool RenameFile(const base::FilePath& old_path,
                  const base::FilePath& new_path,
                  base::Time ctime) {
    DeleteFileRecursively(old_path, ctime);
    return CreateFile(new_path, ctime);
  }

  bool RenameDirectory(const base::FilePath& old_path,
                       const base::FilePath& new_path,
                       base::Time ctime) {
    base::FilePath android_old_path =
        GetAndroidPath(old_path, temp_dir_.GetPath(), android_dir_);
    base::FilePath android_new_path =
        GetAndroidPath(new_path, temp_dir_.GetPath(), android_dir_);

    // Collect all files to be renamed recursively under the |old_path|.
    std::vector<base::FilePath> to_be_renamed = {android_old_path};
    for (const auto& entry : expected_media_store_) {
      if (android_old_path.IsParent(entry.first)) {
        to_be_renamed.push_back(entry.first);
      }
    }

    // Update media store index for all files under |old_path| to be under the
    // |new_path|.
    for (const auto& to_be_renamed_path : to_be_renamed) {
      base::FilePath path = android_new_path;
      android_old_path.AppendRelativePath(to_be_renamed_path, &path);
      expected_media_store_[path] = expected_media_store_[to_be_renamed_path];
      expected_media_store_.erase(to_be_renamed_path);
    }

    // Set the ctime accordingly.
    file_util_.SetLastChangeTime(new_path, ctime);
    ModifyDirectory(new_path.DirName(), ctime);
    return Move(old_path, new_path);
  }

  bool DeleteFileRecursively(const base::FilePath& path, base::Time ctime) {
    base::FilePath android_path =
        GetAndroidPath(path, temp_dir_.GetPath(), android_dir_);

    // Collect all files to be removed recursively under the |path|.
    std::vector<base::FilePath> to_be_removed = {android_path};
    for (const auto& entry : expected_media_store_) {
      if (android_path.IsParent(entry.first)) {
        to_be_removed.push_back(entry.first);
      }
    }

    // Update media store index to remove the collected files.
    for (const auto& to_be_removed_path : to_be_removed) {
      expected_media_store_.erase(to_be_removed_path);
    }

    // Set the ctime accordingly.
    ModifyDirectory(path.DirName(), ctime);
    return base::DeletePathRecursively(path);
  }

  std::unique_ptr<TestingProfileManager> profile_manager_;
  ArcBridgeService arc_bridge_service_;
  base::FilePath android_dir_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<FileSystemScanner> file_system_scanner_;
  content::BrowserTaskEnvironment task_environment_;
  FakeFileSystemInstance file_system_instance_;
  std::unique_ptr<ArcFileSystemBridge> arc_file_system_bridge_;
  std::unique_ptr<TestingProfile> profile_;
  FileUtil file_util_;
  std::map<base::FilePath, base::Time> expected_media_store_;
};

TEST_F(ArcFileSystemScannerTest, ScheduleFullScan) {
  file_system_scanner_->ScheduleFullScan();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(expected_media_store_, file_system_instance_.GetMediaStore());
}

TEST_F(ArcFileSystemScannerTest, ScheduleRegularScanCreateTopLevelFile) {
  file_system_instance_.SetMediaStore(expected_media_store_);

  base::Time now = base::Time::Now();
  base::FilePath dir = temp_dir_.GetPath();

  // Test if the scanner catches creation of top level files.
  ASSERT_TRUE(CreateFile(dir.AppendASCII("foo.png"), now));

  file_system_scanner_->previous_scan_time_ = now;
  file_system_scanner_->ScheduleRegularScan();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(expected_media_store_, file_system_instance_.GetMediaStore());
}

TEST_F(ArcFileSystemScannerTest, ScheduleRegularScanCreateTopLevelDirectory) {
  file_system_instance_.SetMediaStore(expected_media_store_);

  base::Time now = base::Time::Now();
  base::FilePath dir = temp_dir_.GetPath();

  // Test if the scanner catches creation of top level directories.
  ASSERT_TRUE(CreateDirectory(dir.AppendASCII("foo"), now));

  file_system_scanner_->previous_scan_time_ = now;
  file_system_scanner_->ScheduleRegularScan();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(expected_media_store_, file_system_instance_.GetMediaStore());
}

TEST_F(ArcFileSystemScannerTest, ScheduleRegularScanModifyTopLevelFile) {
  file_system_instance_.SetMediaStore(expected_media_store_);

  base::Time now = base::Time::Now();
  base::FilePath dir = temp_dir_.GetPath();

  // Test if the scanner catches creation of top level files.
  ModifyFile(dir.AppendASCII("1.png"), now);

  file_system_scanner_->previous_scan_time_ = now;
  file_system_scanner_->ScheduleRegularScan();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(expected_media_store_, file_system_instance_.GetMediaStore());
}

TEST_F(ArcFileSystemScannerTest, ScheduleRegularScanModifyTopLevelDirectory) {
  file_system_instance_.SetMediaStore(expected_media_store_);

  base::Time now = base::Time::Now();
  base::FilePath dir = temp_dir_.GetPath();

  // Test if the scanner catches creation of top level files.
  ModifyDirectory(dir.AppendASCII("a"), now);

  file_system_scanner_->previous_scan_time_ = now;
  file_system_scanner_->ScheduleRegularScan();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(expected_media_store_, file_system_instance_.GetMediaStore());
}

TEST_F(ArcFileSystemScannerTest, ScheduleRegularScanRenameTopLevelFile) {
  file_system_instance_.SetMediaStore(expected_media_store_);

  base::Time now = base::Time::Now();
  base::FilePath dir = temp_dir_.GetPath();

  // Test if the scanner catches creation of top level files.
  ASSERT_TRUE(
      RenameFile(dir.AppendASCII("1.png"), dir.AppendASCII("foo.jpg"), now));

  file_system_scanner_->previous_scan_time_ = now;
  file_system_scanner_->ScheduleRegularScan();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(expected_media_store_, file_system_instance_.GetMediaStore());
}

TEST_F(ArcFileSystemScannerTest, ScheduleRegularScanRenameTopLevelDirectory) {
  file_system_instance_.SetMediaStore(expected_media_store_);

  base::Time now = base::Time::Now();
  base::FilePath dir = temp_dir_.GetPath();

  // Test if the scanner catches creation of top level files.
  ASSERT_TRUE(
      RenameDirectory(dir.AppendASCII("a"), dir.AppendASCII("foo"), now));

  file_system_scanner_->previous_scan_time_ = now;
  file_system_scanner_->ScheduleRegularScan();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(expected_media_store_, file_system_instance_.GetMediaStore());
}

TEST_F(ArcFileSystemScannerTest, ScheduleRegularScanDeleteTopLevelFile) {
  file_system_instance_.SetMediaStore(expected_media_store_);

  base::FilePath dir = temp_dir_.GetPath();
  base::Time now = base::Time::Now();

  // Test if the scanner catches when top level files are deleted.
  ASSERT_TRUE(DeleteFileRecursively(dir.AppendASCII("1.png"), now));

  file_system_scanner_->previous_scan_time_ = now;
  file_system_scanner_->ScheduleRegularScan();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(expected_media_store_, file_system_instance_.GetMediaStore());
}

TEST_F(ArcFileSystemScannerTest, ScheduleRegularScanDeleteTopLevelDirectory) {
  file_system_instance_.SetMediaStore(expected_media_store_);

  base::FilePath dir = temp_dir_.GetPath();
  base::Time now = base::Time::Now();

  // Test if the scanner catches when top level files are deleted.
  ASSERT_TRUE(DeleteFileRecursively(dir.AppendASCII("a"), now));

  file_system_scanner_->previous_scan_time_ = now;
  file_system_scanner_->ScheduleRegularScan();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(expected_media_store_, file_system_instance_.GetMediaStore());
}

TEST_F(ArcFileSystemScannerTest, ScheduleRegularScanCreateNestedFile) {
  file_system_instance_.SetMediaStore(expected_media_store_);

  base::Time now = base::Time::Now();
  base::FilePath dir = temp_dir_.GetPath();

  // Test if the scanner catches creation of top level files.
  ASSERT_TRUE(CreateFile(dir.AppendASCII("a/foo.png"), now));
  ASSERT_TRUE(CreateFile(dir.AppendASCII("a/b/bar.png"), now));

  file_system_scanner_->previous_scan_time_ = now;
  file_system_scanner_->ScheduleRegularScan();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(expected_media_store_, file_system_instance_.GetMediaStore());
}

TEST_F(ArcFileSystemScannerTest, ScheduleRegularScanCreateNestedDirectory) {
  file_system_instance_.SetMediaStore(expected_media_store_);

  base::Time now = base::Time::Now();
  base::FilePath dir = temp_dir_.GetPath();

  // Test if the scanner catches creation of top level files.
  ASSERT_TRUE(CreateDirectory(dir.AppendASCII("a/foo"), now));
  ASSERT_TRUE(CreateDirectory(dir.AppendASCII("a/b/bar"), now));

  file_system_scanner_->previous_scan_time_ = now;
  file_system_scanner_->ScheduleRegularScan();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(expected_media_store_, file_system_instance_.GetMediaStore());
}

TEST_F(ArcFileSystemScannerTest, ScheduleRegularScanRenameNestedFile) {
  file_system_instance_.SetMediaStore(expected_media_store_);

  base::Time now = base::Time::Now();
  base::FilePath dir = temp_dir_.GetPath();

  // Test if the scanner catches creation of top level files.
  ASSERT_TRUE(RenameFile(dir.AppendASCII("a/2.png"),
                         dir.AppendASCII("a/foo.jpg"), now));
  ASSERT_TRUE(RenameFile(dir.AppendASCII("a/b/3.png"),
                         dir.AppendASCII("a/b/bar.jpg"), now));

  file_system_scanner_->previous_scan_time_ = now;
  file_system_scanner_->ScheduleRegularScan();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(expected_media_store_, file_system_instance_.GetMediaStore());
}

TEST_F(ArcFileSystemScannerTest, ScheduleRegularScanRenameNestedDirectory) {
  file_system_instance_.SetMediaStore(expected_media_store_);

  base::Time now = base::Time::Now();
  base::FilePath dir = temp_dir_.GetPath();

  // Test if the scanner catches creation of top level files.
  ASSERT_TRUE(
      RenameDirectory(dir.AppendASCII("a/b"), dir.AppendASCII("a/foo"), now));

  file_system_scanner_->previous_scan_time_ = now;
  file_system_scanner_->ScheduleRegularScan();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(expected_media_store_, file_system_instance_.GetMediaStore());
}

TEST_F(ArcFileSystemScannerTest, ScheduleRegularScanModifyNestedFile) {
  file_system_instance_.SetMediaStore(expected_media_store_);

  base::Time now = base::Time::Now();
  base::FilePath dir = temp_dir_.GetPath();

  // Test if the scanner catches creation of top level files.
  ModifyFile(dir.AppendASCII("a/2.png"), now);
  ModifyFile(dir.AppendASCII("a/b/3.png"), now);

  file_system_scanner_->previous_scan_time_ = now;
  file_system_scanner_->ScheduleRegularScan();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(expected_media_store_, file_system_instance_.GetMediaStore());
}

TEST_F(ArcFileSystemScannerTest, ScheduleRegularScanModifyNestedDirectory) {
  file_system_instance_.SetMediaStore(expected_media_store_);

  base::Time now = base::Time::Now();
  base::FilePath dir = temp_dir_.GetPath();

  // Test if the scanner catches creation of top level files.
  ModifyDirectory(dir.AppendASCII("a/b"), now);

  file_system_scanner_->previous_scan_time_ = now;
  file_system_scanner_->ScheduleRegularScan();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(expected_media_store_, file_system_instance_.GetMediaStore());
}

TEST_F(ArcFileSystemScannerTest, ScheduleRegularScanDeleteNestedFile) {
  file_system_instance_.SetMediaStore(expected_media_store_);

  base::FilePath dir = temp_dir_.GetPath();
  base::Time now = base::Time::Now();

  // Test if the scanner catches when top level files are deleted.
  ASSERT_TRUE(DeleteFileRecursively(dir.AppendASCII("a/2.png"), now));
  ASSERT_TRUE(DeleteFileRecursively(dir.AppendASCII("a/b/3.png"), now));

  file_system_scanner_->previous_scan_time_ = now;
  file_system_scanner_->ScheduleRegularScan();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(expected_media_store_, file_system_instance_.GetMediaStore());
}

TEST_F(ArcFileSystemScannerTest, ScheduleRegularScanDeleteNestedDirectory) {
  file_system_instance_.SetMediaStore(expected_media_store_);
  // EXPECT_NE(expected_media_store_, file_system_instance_.GetMediaStore());
  base::FilePath dir = temp_dir_.GetPath();
  base::Time now = base::Time::Now();

  // Test if the scanner catches when top level files are deleted.
  ASSERT_TRUE(DeleteFileRecursively(dir.AppendASCII("a/b"), now));

  file_system_scanner_->previous_scan_time_ = now;
  file_system_scanner_->ScheduleRegularScan();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(expected_media_store_, file_system_instance_.GetMediaStore());
}

TEST_F(ArcFileSystemScannerTest, ScheduleRegularScanNoChange) {
  file_system_instance_.SetMediaStore(expected_media_store_);

  base::Time now = base::Time::Now();

  // Test if the scanner works as intended when there are no file system
  // events.
  file_system_scanner_->previous_scan_time_ = now;
  file_system_scanner_->ScheduleRegularScan();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(expected_media_store_, file_system_instance_.GetMediaStore());
}

}  // namespace arc
