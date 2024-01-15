// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/downgrade/user_data_downgrade.h"

#include <optional>

#include "base/containers/flat_set.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_constants.h"
#include "chrome/browser/downgrade/snapshot_file_collector.h"
#include "chrome/common/chrome_constants.h"
#include "content/public/browser/browsing_data_remover.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace downgrade {

TEST(UserDataDowngradeTests, GetInvalidSnapshots) {
  base::ScopedTempDir snapshot_dir;
  ASSERT_TRUE(snapshot_dir.CreateUniqueTempDir());
  base::FilePath snapshot_path = snapshot_dir.GetPath();
  for (const std::string& name : std::vector<std::string>{"10", "11", "30"}) {
    ASSERT_TRUE(base::CreateDirectory(snapshot_path.AppendASCII(name)));
    base::File(
        snapshot_path.AppendASCII(name).Append(kDowngradeLastVersionFile),
        base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  }
  ASSERT_TRUE(base::CreateDirectory(snapshot_path.AppendASCII("20")));
  ASSERT_TRUE(base::CreateDirectory(snapshot_path.AppendASCII("Snapshot 20")));
  base::File(snapshot_path.AppendASCII("Snapshot 20")
                 .Append(kDowngradeLastVersionFile),
             base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  ASSERT_TRUE(base::CreateDirectory(snapshot_path.AppendASCII("Something")));
  auto snapshots = GetInvalidSnapshots(snapshot_path);
  base::flat_set<base::FilePath> expected{
      snapshot_path.AppendASCII("Snapshot 20"), snapshot_path.AppendASCII("20"),
      snapshot_path.AppendASCII("Something")};
  EXPECT_EQ(base::flat_set<base::FilePath>(snapshots), expected);
}

TEST(UserDataDowngradeTests, GetAvailableSnapshots) {
  base::ScopedTempDir snapshot_dir;
  ASSERT_TRUE(snapshot_dir.CreateUniqueTempDir());

  for (const std::string& name :
       std::vector<std::string>{"8", "10.0.0", "11.0.11.123", "30.0.1.1"}) {
    ASSERT_TRUE(
        base::CreateDirectory(snapshot_dir.GetPath().AppendASCII(name)));
    base::File(snapshot_dir.GetPath().AppendASCII(name).Append(
                   kDowngradeLastVersionFile),
               base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  }
  ASSERT_TRUE(
      base::CreateDirectory(snapshot_dir.GetPath().AppendASCII("20.0.2")));
  ASSERT_TRUE(
      base::CreateDirectory(snapshot_dir.GetPath().AppendASCII("Snapshot 20")));
  ASSERT_TRUE(
      base::CreateDirectory(snapshot_dir.GetPath().AppendASCII("Something")));
  base::File(snapshot_dir.GetPath().AppendASCII("9"),
             base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  auto snapshots = GetAvailableSnapshots(snapshot_dir.GetPath());
  base::flat_set<base::Version> expected{
      base::Version("8"), base::Version("10.0.0"), base::Version("11.0.11.123"),
      base::Version("30.0.1.1")};
  EXPECT_EQ(snapshots, expected);
}

TEST(UserDataDowngradeTests, GetSnapshotToRestore) {
  base::ScopedTempDir user_data_dir;
  ASSERT_TRUE(user_data_dir.CreateUniqueTempDir());
  const base::FilePath snapshot_dir =
      user_data_dir.GetPath().Append(kSnapshotsDir);
  ASSERT_TRUE(base::CreateDirectory(snapshot_dir.AppendASCII("9")));
  for (const std::string& name :
       std::vector<std::string>{"10.0.0", "11.3.2", "20.0.0", "30.0.0"}) {
    ASSERT_TRUE(base::CreateDirectory(snapshot_dir.AppendASCII(name)));
    base::File(snapshot_dir.AppendASCII(name).Append(kDowngradeLastVersionFile),
               base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  }

  EXPECT_EQ(GetSnapshotToRestore(base::Version("9"), user_data_dir.GetPath()),
            std::nullopt);
  EXPECT_EQ(
      *GetSnapshotToRestore(base::Version("10.1.0"), user_data_dir.GetPath()),
      base::Version("10.0.0"));
  EXPECT_EQ(
      *GetSnapshotToRestore(base::Version("15.5.3"), user_data_dir.GetPath()),
      base::Version("11.3.2"));
  EXPECT_EQ(
      *GetSnapshotToRestore(base::Version("30.0.0"), user_data_dir.GetPath()),
      base::Version("30.0.0"));
  EXPECT_EQ(
      *GetSnapshotToRestore(base::Version("31.0.0"), user_data_dir.GetPath()),
      base::Version("30.0.0"));
}

TEST(UserDataDowngradeTests, RemoveDataForProfile) {
  base::ScopedTempDir user_data_dir;
  ASSERT_TRUE(user_data_dir.CreateUniqueTempDir());
  const base::FilePath snapshot_dir =
      user_data_dir.GetPath().Append(kSnapshotsDir);
  const auto profile_path_default =
      user_data_dir.GetPath().AppendASCII("Default");
  const auto profile_path_1 = user_data_dir.GetPath().AppendASCII("Profile 1");
  const auto snapshot_profile_path_default =
      snapshot_dir.AppendASCII("1").AppendASCII("Default");
  const auto snapshot_profile_path_1 =
      snapshot_dir.AppendASCII("1").AppendASCII("Profile 1");
  ASSERT_TRUE(base::CreateDirectory(profile_path_default));
  ASSERT_TRUE(base::CreateDirectory(profile_path_1));
  ASSERT_TRUE(base::CreateDirectory(snapshot_profile_path_default));
  ASSERT_TRUE(base::CreateDirectory(snapshot_profile_path_1));
  base::File(profile_path_default.Append(kDowngradeLastVersionFile),
             base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  base::File(profile_path_1.Append(kDowngradeLastVersionFile),
             base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  base::File(snapshot_dir.AppendASCII("1").Append(kDowngradeLastVersionFile),
             base::File::FLAG_CREATE | base::File::FLAG_WRITE);

  const auto profile_items = CollectProfileItems();
  DCHECK(!profile_items.empty());
  for (const auto& item : profile_items) {
    if (item.is_directory) {
      ASSERT_TRUE(base::CreateDirectory(
          snapshot_profile_path_default.Append(item.path)));
      ASSERT_TRUE(
          base::CreateDirectory(snapshot_profile_path_1.Append(item.path)));
    } else {
      base::File(snapshot_profile_path_default.Append(item.path),
                 base::File::FLAG_CREATE | base::File::FLAG_WRITE);
      base::File(snapshot_profile_path_1.Append(item.path),
                 base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    }
  }

  base::File::Info snapshot_info;
  ASSERT_TRUE(base::GetFileInfo(snapshot_dir, &snapshot_info));

  // Nothing should be deleted from |profile_path_default| since delete_begin
  // is after the snapshot has been created.
  RemoveDataForProfile(base::Time::Max(), profile_path_default,
                       chrome_browsing_data_remover::DATA_TYPE_BOOKMARKS);
  // Only the bookmarks should be deleted.
  RemoveDataForProfile(base::Time::Min(), profile_path_1,
                       chrome_browsing_data_remover::DATA_TYPE_BOOKMARKS);
  EXPECT_TRUE(base::PathExists(
      snapshot_profile_path_default.Append(chrome::kPreferencesFilename)));
  EXPECT_TRUE(base::PathExists(
      snapshot_profile_path_1.Append(chrome::kPreferencesFilename)));
  EXPECT_TRUE(base::PathExists(snapshot_profile_path_default.Append(
      chrome::kSecurePreferencesFilename)));
  EXPECT_TRUE(base::PathExists(
      snapshot_profile_path_1.Append(chrome::kSecurePreferencesFilename)));

  for (const auto& item : profile_items) {
    EXPECT_TRUE(
        base::PathExists(snapshot_profile_path_default.Append(item.path)));
    EXPECT_EQ((item.data_types &
               chrome_browsing_data_remover::DATA_TYPE_BOOKMARKS) == 0ULL,
              base::PathExists(snapshot_profile_path_1.Append(item.path)));
  }

  const auto remove_mask =
      content::BrowsingDataRemover::DATA_TYPE_COOKIES |
      chrome_browsing_data_remover::DATA_TYPE_ISOLATED_ORIGINS |
      chrome_browsing_data_remover::DATA_TYPE_HISTORY |
      chrome_browsing_data_remover::DATA_TYPE_CONTENT_SETTINGS |
      chrome_browsing_data_remover::DATA_TYPE_PASSWORDS |
      chrome_browsing_data_remover::DATA_TYPE_FORM_DATA;

  // Delete some data from default profile.
  RemoveDataForProfile(base::Time::Min(), profile_path_default, remove_mask);
  for (const auto& item : profile_items) {
    EXPECT_EQ(
        (item.data_types & remove_mask) == 0ULL,
        base::PathExists(snapshot_profile_path_default.Append(item.path)));
  }
  // Wipe profile 1
  RemoveDataForProfile(base::Time(), profile_path_1,
                       chrome_browsing_data_remover::WIPE_PROFILE);
  EXPECT_FALSE(base::PathExists(snapshot_profile_path_1));
}

}  // namespace downgrade
