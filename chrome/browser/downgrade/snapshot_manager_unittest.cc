// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/downgrade/snapshot_manager.h"

#include "base/containers/adapters.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/downgrade/downgrade_utils.h"
#include "chrome/browser/downgrade/snapshot_file_collector.h"
#include "chrome/browser/downgrade/user_data_downgrade.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace downgrade {

namespace {

constexpr base::FilePath::StringPieceType kSomeFolder =
    FILE_PATH_LITERAL("Some Folder");
constexpr base::FilePath::StringPieceType kSomeFile =
    FILE_PATH_LITERAL("Some File");
constexpr base::FilePath::StringPieceType kSomeFolderFile =
    FILE_PATH_LITERAL("A File");
constexpr base::FilePath::StringPieceType kSomeSubFolder =
    FILE_PATH_LITERAL("Some Sub Folder");
constexpr base::FilePath::StringPieceType kSomeSubFile =
    FILE_PATH_LITERAL("Some Sub File");

constexpr base::FilePath::StringPieceType kUserDataFolder =
    FILE_PATH_LITERAL("User Data Folder");
constexpr base::FilePath::StringPieceType kProfileDataFolder =
    FILE_PATH_LITERAL("Profile Data Folder");
constexpr base::FilePath::StringPieceType kUserDataFile =
    FILE_PATH_LITERAL("User Data File");
constexpr base::FilePath::StringPieceType kProfileDataFile =
    FILE_PATH_LITERAL("Profile Data File");
constexpr base::FilePath::StringPieceType kProfileDataJournalFile =
    FILE_PATH_LITERAL("Profile Data File-journal");
constexpr base::FilePath::StringPieceType kProfileDataExtFile =
    FILE_PATH_LITERAL("Profile Data File.ext");
constexpr base::FilePath::StringPieceType kProfileDataExtWalFile =
    FILE_PATH_LITERAL("Profile Data File.ext-wal");
constexpr base::FilePath::StringPieceType kProfileDataExtShmFile =
    FILE_PATH_LITERAL("Profile Data File.ext-shm");

constexpr std::array<base::FilePath::StringPieceType, 3>
    kProfileDirectoryBaseNames = {FILE_PATH_LITERAL("Default"),
                                  FILE_PATH_LITERAL("Profile 1"),
                                  FILE_PATH_LITERAL("Profile 2")};

// Structure containing a folders and files structure for tests.
// root
// |_ SomeFile
// |_ Some Folder
//    |_ Some File
//    |_ Some Sub Folder
//       |_ Some Sub File
class TestFolderAndFiles {
 public:
  // Creates the files and folders under the provided |root|.
  // Cleanup must be done manually.
  static void CreateFilesAndFolders(const base::FilePath& root) {
    TestFolderAndFiles folders_and_files(root);
    ASSERT_TRUE(base::CreateDirectory(folders_and_files.some_sub_folder_path_));
    base::File(folders_and_files.some_file_path_,
               base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    base::File(folders_and_files.some_folder_file_path_,
               base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    base::File(folders_and_files.some_sub_folder_file_path_,
               base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  }

  static bool AllPathExists(const base::FilePath& root) {
    TestFolderAndFiles folders_and_files(root);
    return base::DirectoryExists(folders_and_files.some_folder_path_) &&
           base::DirectoryExists(folders_and_files.some_sub_folder_path_) &&
           base::PathExists(folders_and_files.some_file_path_) &&
           base::PathExists(folders_and_files.some_folder_file_path_) &&
           base::PathExists(folders_and_files.some_sub_folder_file_path_);
  }

  static bool NoPathExists(const base::FilePath& root) {
    TestFolderAndFiles folders_and_files(root);
    return !base::DirectoryExists(folders_and_files.some_folder_path_) &&
           !base::DirectoryExists(folders_and_files.some_sub_folder_path_) &&
           !base::PathExists(folders_and_files.some_file_path_) &&
           !base::PathExists(folders_and_files.some_folder_file_path_) &&
           !base::PathExists(folders_and_files.some_sub_folder_file_path_);
  }

 private:
  explicit TestFolderAndFiles(const base::FilePath& root) {
    some_folder_path_ = root.Append(kSomeFolder);
    some_folder_file_path_ = some_folder_path_.Append(kSomeFile);
    some_file_path_ = some_folder_path_.Append(kSomeFolderFile);
    some_sub_folder_path_ = some_folder_path_.Append(kSomeSubFolder);
    some_sub_folder_file_path_ = some_sub_folder_path_.Append(kSomeSubFile);
  }
  ~TestFolderAndFiles() = default;

  base::FilePath some_folder_path_;
  base::FilePath some_file_path_;
  base::FilePath some_folder_file_path_;
  base::FilePath some_sub_folder_path_;
  base::FilePath some_sub_folder_file_path_;
};

}  // namespace

class TestSnapshotManager : public SnapshotManager {
 public:
  explicit TestSnapshotManager(base::FilePath path) : SnapshotManager(path) {}
  ~TestSnapshotManager() = default;

 private:
  std::vector<SnapshotItemDetails> GetUserSnapshotItemDetails() const override {
    return std::vector<SnapshotItemDetails>{
        SnapshotItemDetails(base::FilePath(kUserDataFile),
                            SnapshotItemDetails::ItemType::kFile, 0,
                            SnapshotItemId::kMaxValue),
        SnapshotItemDetails(base::FilePath(kUserDataFolder),
                            SnapshotItemDetails::ItemType::kDirectory, 0,
                            SnapshotItemId::kMaxValue)};
  }
  std::vector<SnapshotItemDetails> GetProfileSnapshotItemDetails()
      const override {
    return std::vector<SnapshotItemDetails>{
        SnapshotItemDetails(base::FilePath(kProfileDataFile),
                            SnapshotItemDetails::ItemType::kFile, 0,
                            SnapshotItemId::kMaxValue),
        SnapshotItemDetails(base::FilePath(kProfileDataExtFile),
                            SnapshotItemDetails::ItemType::kFile, 0,
                            SnapshotItemId::kMaxValue),
        SnapshotItemDetails(base::FilePath(kProfileDataFolder),
                            SnapshotItemDetails::ItemType::kDirectory, 0,
                            SnapshotItemId::kMaxValue)};
  }
};

class SnapshotManagerTest : public testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(user_data_dir_.CreateUniqueTempDir()); }

  void TearDown() override { EXPECT_TRUE(user_data_dir_.Delete()); }

  base::FilePath user_data_dir() const { return user_data_dir_.GetPath(); }

  void CreateProfileDirectories() {
    for (const auto& path : kProfileDirectoryBaseNames) {
      ASSERT_TRUE(base::CreateDirectory(user_data_dir().Append(path)));
      absolute_profile_directories_.push_back(user_data_dir().Append(path));
    }
  }

  const std::vector<base::FilePath>& absolute_profile_directories() const {
    return absolute_profile_directories_;
  }

  base::FilePath GetSnapshotDirectory(const base::Version& version) const {
    return user_data_dir()
        .Append(kSnapshotsDir)
        .AppendASCII(version.GetString());
  }

 private:
  base::ScopedTempDir user_data_dir_;
  std::vector<base::FilePath> absolute_profile_directories_;
};

TEST_F(SnapshotManagerTest, TakeSnapshot) {
  base::Version version("10.0.0");

  ASSERT_NO_FATAL_FAILURE(CreateProfileDirectories());
  const auto& absolute_profile_paths = absolute_profile_directories();

  // Files and folders at User Data level that should not be snapshotted.
  ASSERT_NO_FATAL_FAILURE(
      TestFolderAndFiles::CreateFilesAndFolders(user_data_dir()));

  // Files and folders at User Data level that should be snapshotted.
  base::File user_data_file(user_data_dir().Append(kUserDataFile),
                            base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  ASSERT_NO_FATAL_FAILURE(TestFolderAndFiles::CreateFilesAndFolders(
      user_data_dir().Append(kUserDataFolder)));

  for (const auto& path : absolute_profile_paths) {
    // Files and folders at Profile Data level that should be snapshotted.
    base::File file(path.Append(kProfileDataFile),
                    base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    base::File file_ext(path.Append(kProfileDataExtFile),
                        base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    base::File file_journal(path.Append(kProfileDataJournalFile),
                            base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    base::File file_ext_wal(path.Append(kProfileDataExtWalFile),
                            base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    base::File file_ext_shm(path.Append(kProfileDataExtShmFile),
                            base::File::FLAG_CREATE | base::File::FLAG_WRITE);

    ASSERT_NO_FATAL_FAILURE(TestFolderAndFiles::CreateFilesAndFolders(
        path.Append(kProfileDataFolder)));

    // Files and folders at Profile Data level that should not be snapshotted.
    ASSERT_NO_FATAL_FAILURE(TestFolderAndFiles::CreateFilesAndFolders(path));
  }

  TestSnapshotManager snapshot_manager(user_data_dir());
  snapshot_manager.TakeSnapshot(version);

  auto snapshot_dir = GetSnapshotDirectory(version);

  EXPECT_TRUE(base::PathExists(user_data_dir().Append(kUserDataFile)));
  EXPECT_TRUE(base::DirectoryExists(user_data_dir().Append(kUserDataFolder)));
  EXPECT_TRUE(base::PathExists(snapshot_dir.Append(kUserDataFile)));
  EXPECT_TRUE(base::DirectoryExists(snapshot_dir.Append(kUserDataFolder)));
  EXPECT_TRUE(TestFolderAndFiles::AllPathExists(user_data_dir()));
  EXPECT_TRUE(TestFolderAndFiles::NoPathExists(snapshot_dir));

  for (const auto& path : absolute_profile_paths) {
    EXPECT_TRUE(base::PathExists(path.Append(kProfileDataFile)));
    EXPECT_TRUE(base::PathExists(path.Append(kProfileDataExtFile)));
    EXPECT_TRUE(base::PathExists(path.Append(kProfileDataJournalFile)));
    EXPECT_TRUE(base::PathExists(path.Append(kProfileDataExtWalFile)));
    EXPECT_TRUE(base::PathExists(path.Append(kProfileDataExtShmFile)));
    EXPECT_TRUE(base::DirectoryExists(path.Append(kProfileDataFolder)));
    EXPECT_TRUE(TestFolderAndFiles::AllPathExists(path));
  }

  for (const auto& path : kProfileDirectoryBaseNames) {
    EXPECT_TRUE(
        base::PathExists(snapshot_dir.Append(path).Append(kProfileDataFile)));
    EXPECT_TRUE(base::PathExists(
        snapshot_dir.Append(path).Append(kProfileDataJournalFile)));
    EXPECT_TRUE(base::PathExists(
        snapshot_dir.Append(path).Append(kProfileDataExtFile)));
    EXPECT_TRUE(base::PathExists(
        snapshot_dir.Append(path).Append(kProfileDataExtWalFile)));
    EXPECT_TRUE(base::PathExists(
        snapshot_dir.Append(path).Append(kProfileDataExtShmFile)));
    EXPECT_TRUE(base::DirectoryExists(
        snapshot_dir.Append(path).Append(kProfileDataFolder)));
    EXPECT_TRUE(TestFolderAndFiles::NoPathExists(snapshot_dir.Append(path)));
    EXPECT_TRUE(TestFolderAndFiles::AllPathExists(
        snapshot_dir.Append(path).Append(kProfileDataFolder)));
  }
}

TEST_F(SnapshotManagerTest, RestoreSnapshotOlderVersionAvailable) {
  base::Version older_version("9.0.0");
  base::Version existing_version("10.0.0");

  auto older_snapshot_dir = GetSnapshotDirectory(older_version);
  auto existing_snapshot_dir = GetSnapshotDirectory(existing_version);

  ASSERT_TRUE(base::CreateDirectory(older_snapshot_dir));
  ASSERT_NO_FATAL_FAILURE(
      TestFolderAndFiles::CreateFilesAndFolders(existing_snapshot_dir));

  base::File(older_snapshot_dir.Append(kDowngradeLastVersionFile),
             base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  base::File(existing_snapshot_dir.Append(kDowngradeLastVersionFile),
             base::File::FLAG_CREATE | base::File::FLAG_WRITE);

  for (const auto& profile : kProfileDirectoryBaseNames) {
    const base::FilePath profile_folder = existing_snapshot_dir.Append(profile);
    ASSERT_NO_FATAL_FAILURE(
        TestFolderAndFiles::CreateFilesAndFolders(profile_folder));
  }

  SnapshotManager snapshot_manager(user_data_dir());
  snapshot_manager.RestoreSnapshot(base::Version("11.0.0"));

  EXPECT_TRUE(TestFolderAndFiles::AllPathExists(user_data_dir()));
  EXPECT_TRUE(TestFolderAndFiles::AllPathExists(existing_snapshot_dir));

  for (const auto& profile : kProfileDirectoryBaseNames) {
    EXPECT_TRUE(
        TestFolderAndFiles::AllPathExists(user_data_dir().Append(profile)));
    EXPECT_TRUE(TestFolderAndFiles::AllPathExists(
        existing_snapshot_dir.Append(profile)));
  }
}

TEST_F(SnapshotManagerTest, RestoreSnapshotTargetVersionAvailable) {
  base::Version older_version("9.0.0");
  base::Version version("10.0.0");

  auto existing_snapshot_dir = GetSnapshotDirectory(version);
  auto older_snapshot_dir = GetSnapshotDirectory(older_version);

  ASSERT_TRUE(base::CreateDirectory(older_snapshot_dir));
  ASSERT_NO_FATAL_FAILURE(
      TestFolderAndFiles::CreateFilesAndFolders(existing_snapshot_dir));

  base::File(older_snapshot_dir.Append(kDowngradeLastVersionFile),
             base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  base::File(existing_snapshot_dir.Append(kDowngradeLastVersionFile),
             base::File::FLAG_CREATE | base::File::FLAG_WRITE);

  for (const auto& profile : kProfileDirectoryBaseNames) {
    const base::FilePath profile_folder = existing_snapshot_dir.Append(profile);
    ASSERT_NO_FATAL_FAILURE(
        TestFolderAndFiles::CreateFilesAndFolders(profile_folder));
  }

  SnapshotManager snapshot_manager(user_data_dir());
  snapshot_manager.RestoreSnapshot(version);

  EXPECT_TRUE(TestFolderAndFiles::AllPathExists(user_data_dir()));
  EXPECT_TRUE(TestFolderAndFiles::NoPathExists(existing_snapshot_dir));
  EXPECT_TRUE(
      base::PathExists(user_data_dir().Append(kDowngradeLastVersionFile)));

  for (const auto& profile : kProfileDirectoryBaseNames) {
    EXPECT_TRUE(
        TestFolderAndFiles::AllPathExists(user_data_dir().Append(profile)));
    EXPECT_TRUE(TestFolderAndFiles::NoPathExists(
        existing_snapshot_dir.Append(profile)));
  }
}

TEST_F(SnapshotManagerTest, RestoreSnapshotIgnoresIncompleteSnapshots) {
  base::Version older_version("9.0.0");
  base::Version existing_version("10.0.0");
  base::Version newer_version("11.0.0");

  auto older_snapshot_dir = GetSnapshotDirectory(older_version);
  auto existing_snapshot_dir = GetSnapshotDirectory(existing_version);
  auto newer_snapshot_dir = GetSnapshotDirectory(newer_version);

  ASSERT_NO_FATAL_FAILURE(
      TestFolderAndFiles::CreateFilesAndFolders(older_snapshot_dir));
  ASSERT_NO_FATAL_FAILURE(
      TestFolderAndFiles::CreateFilesAndFolders(existing_snapshot_dir));
  ASSERT_NO_FATAL_FAILURE(
      TestFolderAndFiles::CreateFilesAndFolders(newer_snapshot_dir));

  // Only the older directory is a complete snapshot.
  base::File(older_snapshot_dir.Append(kDowngradeLastVersionFile),
             base::File::FLAG_CREATE | base::File::FLAG_WRITE);

  for (const auto& profile : kProfileDirectoryBaseNames) {
    const base::FilePath profile_folder = older_snapshot_dir.Append(profile);
    ASSERT_NO_FATAL_FAILURE(
        TestFolderAndFiles::CreateFilesAndFolders(profile_folder));
  }

  SnapshotManager snapshot_manager(user_data_dir());
  snapshot_manager.RestoreSnapshot(newer_version);

  EXPECT_TRUE(TestFolderAndFiles::AllPathExists(user_data_dir()));
  EXPECT_TRUE(TestFolderAndFiles::AllPathExists(older_snapshot_dir));
  EXPECT_TRUE(TestFolderAndFiles::AllPathExists(existing_snapshot_dir));
  EXPECT_TRUE(TestFolderAndFiles::AllPathExists(newer_snapshot_dir));
  EXPECT_TRUE(
      base::PathExists(user_data_dir().Append(kDowngradeLastVersionFile)));

  for (const auto& profile : kProfileDirectoryBaseNames) {
    EXPECT_TRUE(
        TestFolderAndFiles::AllPathExists(user_data_dir().Append(profile)));
    EXPECT_TRUE(
        TestFolderAndFiles::AllPathExists(older_snapshot_dir.Append(profile)));
  }
}

TEST_F(SnapshotManagerTest, PurgeInvalidAndOldSnapshotsKeepsMaxValidSnapshots) {
  std::vector<base::FilePath> invalid_snapshot_paths{
      user_data_dir().Append(kSnapshotsDir).AppendASCII("Bad format"),
      user_data_dir().Append(kSnapshotsDir).AppendASCII("10"),
  };
  for (const auto& path : invalid_snapshot_paths)
    ASSERT_TRUE(base::CreateDirectory(path));

  std::vector<base::FilePath> valid_snapshot_paths{
      user_data_dir().Append(kSnapshotsDir).AppendASCII("20"),
      user_data_dir().Append(kSnapshotsDir).AppendASCII("30"),
      user_data_dir().Append(kSnapshotsDir).AppendASCII("40"),
      user_data_dir().Append(kSnapshotsDir).AppendASCII("50"),
  };

  for (const auto& path : valid_snapshot_paths) {
    ASSERT_TRUE(base::CreateDirectory(path));
    base::File(path.Append(kDowngradeLastVersionFile),
               base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  }

  int max_number_of_snapshots = 3;
  SnapshotManager snapshot_manager(user_data_dir());
  snapshot_manager.PurgeInvalidAndOldSnapshots(max_number_of_snapshots,
                                               std::nullopt);

  const base::FilePath deletion_directory =
      user_data_dir()
          .Append(kSnapshotsDir)
          .AddExtension(kDowngradeDeleteSuffix);

  // All invalid snapshots have been movec
  for (const auto& path : invalid_snapshot_paths) {
    EXPECT_FALSE(base::PathExists(path));
    EXPECT_TRUE(base::PathExists(deletion_directory.Append(path.BaseName())));
  }

  // Only 3 valid snapshots remains
  for (const base::FilePath& path : base::Reversed(valid_snapshot_paths)) {
    EXPECT_EQ(base::PathExists(path), max_number_of_snapshots != 0);
    EXPECT_EQ(!base::PathExists(deletion_directory.Append(path.BaseName())),
              max_number_of_snapshots != 0);
    --max_number_of_snapshots;
  }
}

TEST_F(SnapshotManagerTest, PurgeInvalidAndOldSnapshotsKeepsValidSnapshots) {
  std::vector<base::FilePath> valid_snapshot_paths{
      user_data_dir().Append(kSnapshotsDir).AppendASCII("20"),
      user_data_dir().Append(kSnapshotsDir).AppendASCII("30"),
  };

  for (const auto& path : valid_snapshot_paths) {
    ASSERT_TRUE(base::CreateDirectory(path));
    base::File(path.Append(kDowngradeLastVersionFile),
               base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  }

  int max_number_of_snapshots = 3;
  SnapshotManager snapshot_manager(user_data_dir());
  snapshot_manager.PurgeInvalidAndOldSnapshots(max_number_of_snapshots,
                                               std::nullopt);

  for (const auto& path : valid_snapshot_paths)
    EXPECT_TRUE(base::PathExists(path));
}

TEST_F(SnapshotManagerTest,
       PurgeInvalidAndOldSnapshotsKeepsValidSnapshotsPerMilestone) {
  std::vector<base::FilePath> valid_snapshot_paths{
      user_data_dir().Append(kSnapshotsDir).AppendASCII("19.0.0"),
      user_data_dir().Append(kSnapshotsDir).AppendASCII("20.0.0"),
      user_data_dir().Append(kSnapshotsDir).AppendASCII("20.0.1"),
      user_data_dir().Append(kSnapshotsDir).AppendASCII("21.0.1"),
  };

  for (const auto& path : valid_snapshot_paths) {
    ASSERT_TRUE(base::CreateDirectory(path));
    base::File(path.Append(kDowngradeLastVersionFile),
               base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  }

  int max_number_of_snapshots = 1;
  SnapshotManager snapshot_manager(user_data_dir());
  snapshot_manager.PurgeInvalidAndOldSnapshots(max_number_of_snapshots, 20);

  EXPECT_TRUE(base::PathExists(valid_snapshot_paths[0]));
  EXPECT_FALSE(base::PathExists(valid_snapshot_paths[1]));
  EXPECT_TRUE(base::PathExists(valid_snapshot_paths[2]));
  EXPECT_TRUE(base::PathExists(valid_snapshot_paths[3]));
}

}  // namespace downgrade
