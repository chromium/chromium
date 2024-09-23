// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/sparky/sparky_delegate_impl.h"

#include <memory>
#include <optional>

#include "ash/constants/ash_pref_names.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/system/sys_info.h"
#include "base/test/bind.h"
#include "base/test/scoped_running_on_chromeos.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/spaced/spaced_client.h"
#include "chromeos/ash/components/disks/disk_mount_manager.h"
#include "chromeos/ash/components/disks/fake_disk_mount_manager.h"
#include "chromeos/ash/components/sparky/sparky_util.h"
#include "components/manta/sparky/sparky_delegate.h"
#include "components/manta/sparky/system_info_delegate.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/text/bytes_formatting.h"

namespace ash {

namespace {
using SettingsPrivatePrefType = extensions::api::settings_private::PrefType;
using ::testing::UnorderedElementsAre;

MATCHER_P2(File, path, name, "File matches") {
  return arg.path == path && arg.name == name;
}

MATCHER_P5(FileWithSummary,
           path,
           name,
           summary,
           date_modified,
           size_in_bytes,
           "File with summary matches") {
  return arg.path == path && arg.name == name && arg.summary == summary &&
         arg.date_modified == date_modified &&
         arg.size_in_bytes == size_in_bytes;
}

// Get the path to file manager's test data directory.
base::FilePath GetTestDataFilePath(const std::string& file_name) {
  // Get the path to file manager's test data directory.
  base::FilePath source_dir;
  CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_dir));
  base::FilePath test_data_dir = source_dir.AppendASCII("chrome")
                                     .AppendASCII("test")
                                     .AppendASCII("data")
                                     .AppendASCII("chromeos")
                                     .AppendASCII("file_manager");

  // Return full test data path to the given |file_name|.
  return test_data_dir.Append(base::FilePath::FromUTF8Unsafe(file_name));
}

// Copy a file from the file manager's test data directory to the specified
// target_path.
void AddFile(const std::string& file_name,
             int64_t expected_size,
             base::FilePath target_path) {
  const base::FilePath entry_path = GetTestDataFilePath(file_name);
  target_path = target_path.AppendASCII(file_name);
  ASSERT_TRUE(base::CopyFile(entry_path, target_path))
      << "Copy from " << entry_path.value() << " to " << target_path.value()
      << " failed.";
  // Verify file size.
  base::stat_wrapper_t stat;
  const int res = base::File::Lstat(target_path, &stat);
  ASSERT_FALSE(res < 0) << "Couldn't stat" << target_path.value();
  ASSERT_EQ(expected_size, stat.st_size);
}
}  // namespace

class SparkyDelegateImplTest : public testing::Test {
 public:
  SparkyDelegateImplTest() = default;

  SparkyDelegateImplTest(const SparkyDelegateImplTest&) = delete;
  SparkyDelegateImplTest& operator=(const SparkyDelegateImplTest&) = delete;

  ~SparkyDelegateImplTest() override = default;

  SparkyDelegateImpl* GetSparkyDelegateImpl() {
    return sparky_delegate_impl_.get();
  }

  const base::Value& GetPref(const std::string& setting_id) {
    return profile_->GetPrefs()->GetValue(setting_id);
  }

  void SetBool(const std::string& setting_id, bool bool_val) {
    profile_->GetPrefs()->SetBoolean(setting_id, bool_val);
  }

  void AddToMap(const std::string& pref_name,
                SettingsPrivatePrefType settings_pref_type,
                std::optional<base::Value> value) {
    sparky_delegate_impl_->AddPrefToMap(pref_name, settings_pref_type,
                                        std::move(value));
  }

  SparkyDelegateImpl::SettingsDataList* GetCurrentPrefs() {
    return &sparky_delegate_impl_->current_prefs_;
  }

  // testing::Test:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    sparky_delegate_impl_ =
        std::make_unique<SparkyDelegateImpl>(profile_.get());

    // Initialize fake DBus clients.
    ash::ConciergeClient::InitializeFake(/*fake_cicerone_client=*/nullptr);
    ash::SpacedClient::InitializeFake();

    ash::disks::DiskMountManager::InitializeForTesting(
        new ash::disks::FakeDiskMountManager);

    // Create and register MyFiles directory.
    // By emulating chromeos running, GetMyFilesFolderForProfile will return the
    // profile's temporary location instead of $HOME/Downloads.
    base::test::ScopedRunningOnChromeOS running_on_chromeos;
    const base::FilePath my_files_path =
        file_manager::util::GetMyFilesFolderForProfile(profile_.get());
    CHECK(base::CreateDirectory(my_files_path));
    CHECK(storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
        file_manager::util::GetDownloadsMountPointName(profile_.get()),
        storage::kFileSystemTypeLocal, storage::FileSystemMountOption(),
        my_files_path));

    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
    sparky_delegate_impl_->SetRootPathForTesting(scoped_temp_dir_.GetPath());

    RunUntilIdle();
  }

  void TearDown() override {
    sparky_delegate_impl_.reset();
    profile_.reset();
    ash::disks::DiskMountManager::Shutdown();
    storage::ExternalMountPoints::GetSystemInstance()->RevokeAllFileSystems();
    ash::SpacedClient::Shutdown();
    ash::ConciergeClient::Shutdown();
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 protected:
  base::FilePath Path(const std::string& filename) {
    return scoped_temp_dir_.GetPath().Append(filename);
  }

  void WriteFile(const std::string& filename, const std::string& data) {
    ASSERT_TRUE(base::WriteFile(Path(filename), data));
    ASSERT_TRUE(base::PathExists(Path(filename)));
    RunUntilIdle();
  }

  void CreateDirectory(const std::string& directory_name) {
    ASSERT_TRUE(base::CreateDirectory(Path(directory_name)));
    ASSERT_TRUE(base::PathExists(Path(directory_name)));
    RunUntilIdle();
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;

 private:
  std::unique_ptr<SparkyDelegateImpl> sparky_delegate_impl_;
  base::ScopedTempDir scoped_temp_dir_;
};

TEST_F(SparkyDelegateImplTest, SetSettings) {
  SetBool(prefs::kDarkModeEnabled, false);
  SetBool(prefs::kPowerAdaptiveChargingEnabled, true);
  ASSERT_TRUE(GetSparkyDelegateImpl()->SetSettings(
      std::make_unique<manta::SettingsData>(prefs::kDarkModeEnabled,
                                            manta::PrefType::kBoolean,
                                            base::Value(true))));
  ASSERT_TRUE(GetSparkyDelegateImpl()->SetSettings(
      std::make_unique<manta::SettingsData>(
          prefs::kPowerAdaptiveChargingEnabled, manta::PrefType::kBoolean,
          base::Value(false))));
  const base::Value& dark_mode_val = GetPref(prefs::kDarkModeEnabled);
  const base::Value& adaptive_charging_val =
      GetPref(prefs::kPowerAdaptiveChargingEnabled);
  RunUntilIdle();
  ASSERT_TRUE(dark_mode_val.is_bool());
  ASSERT_TRUE(dark_mode_val.GetBool());
  ASSERT_TRUE(adaptive_charging_val.is_bool());
  ASSERT_FALSE(adaptive_charging_val.GetBool());
}

TEST_F(SparkyDelegateImplTest, AddPrefToMap) {
  AddToMap("bool pref", SettingsPrivatePrefType::kBoolean,
           std::make_optional<base::Value>(true));
  AddToMap("int pref", SettingsPrivatePrefType::kNumber,
           std::make_optional<base::Value>(1));
  AddToMap("double pref", SettingsPrivatePrefType::kNumber,
           std::make_optional<base::Value>(0.5));
  AddToMap("string pref", SettingsPrivatePrefType::kString,
           std::make_optional<base::Value>("my string"));
  RunUntilIdle();
  ASSERT_TRUE(GetCurrentPrefs()->contains("bool pref"));
  ASSERT_TRUE(GetCurrentPrefs()->contains("int pref"));
  ASSERT_TRUE(GetCurrentPrefs()->contains("double pref"));
  ASSERT_TRUE(GetCurrentPrefs()->contains("string pref"));
  ASSERT_EQ(GetCurrentPrefs()->find("bool pref")->second->pref_name,
            "bool pref");
  ASSERT_EQ(GetCurrentPrefs()->find("int pref")->second->pref_name, "int pref");
  ASSERT_EQ(GetCurrentPrefs()->find("double pref")->second->pref_name,
            "double pref");
  ASSERT_EQ(GetCurrentPrefs()->find("string pref")->second->pref_name,
            "string pref");
  ASSERT_EQ(GetCurrentPrefs()->find("bool pref")->second->pref_type,
            manta::PrefType::kBoolean);
  ASSERT_EQ(GetCurrentPrefs()->find("int pref")->second->pref_type,
            manta::PrefType::kInt);
  ASSERT_EQ(GetCurrentPrefs()->find("double pref")->second->pref_type,
            manta::PrefType::kDouble);
  ASSERT_EQ(GetCurrentPrefs()->find("string pref")->second->pref_type,
            manta::PrefType::kString);
  ASSERT_TRUE(GetCurrentPrefs()->find("bool pref")->second->bool_val);
  ASSERT_EQ(GetCurrentPrefs()->find("int pref")->second->int_val, 1);
  ASSERT_EQ(GetCurrentPrefs()->find("double pref")->second->double_val, 0.5);
  ASSERT_EQ(GetCurrentPrefs()->find("string pref")->second->string_val,
            "my string");
}

TEST_F(SparkyDelegateImplTest, ObtainStorageInfo) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  // Get local filesystem storage statistics.
  const base::FilePath mount_path =
      file_manager::util::GetMyFilesFolderForProfile(profile_.get());
  const base::FilePath downloads_path =
      file_manager::util::GetDownloadsFolderForProfile(profile_.get());

  const base::FilePath android_files_path =
      profile_->GetPath().Append("AndroidFiles");
  const base::FilePath android_files_download_path =
      android_files_path.Append("Download");

  // Create directories.
  CHECK(base::CreateDirectory(downloads_path));
  CHECK(base::CreateDirectory(android_files_path));

  // Register android files mount point.
  CHECK(storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
      file_manager::util::GetAndroidFilesMountPointName(),
      storage::kFileSystemTypeLocal, storage::FileSystemMountOption(),
      android_files_path));

  const int kMountPathBytes = 8092;
  const int kAndroidPathBytes = 15271;
  const int kDownloadsPathBytes = 56758;

  // Add files in MyFiles and Android files.
  AddFile("random.bin", kMountPathBytes, mount_path);          // ~7.9 KB
  AddFile("tall.pdf", kAndroidPathBytes, android_files_path);  // ~14.9 KB
  // Add file in Downloads and simulate bind mount with
  // [android files]/Download.
  AddFile("video.ogv", kDownloadsPathBytes, downloads_path);  // ~55.4 KB

  int64_t total_bytes = base::SysInfo::AmountOfTotalDiskSpace(mount_path);
  int64_t available_bytes = base::SysInfo::AmountOfFreeDiskSpace(mount_path);
  int64_t rounded_total_size = sparky::RoundByteSize(total_bytes);

  std::string available_size =
      base::UTF16ToUTF8(ui::FormatBytes(available_bytes));
  std::string total_size =
      base::UTF16ToUTF8(ui::FormatBytes(rounded_total_size));
  auto quit_closure = task_environment_.QuitClosure();

  GetSparkyDelegateImpl()->ObtainStorageInfo(base::BindLambdaForTesting(
      [&quit_closure, this, available_size,
       total_size](std::unique_ptr<manta::StorageData> storage_data) {
        RunUntilIdle();
        ASSERT_TRUE(storage_data);
        ASSERT_EQ(storage_data->free_bytes, available_size);
        ASSERT_EQ(storage_data->total_bytes, total_size);
        quit_closure.Run();
      }));
}

TEST_F(SparkyDelegateImplTest, GetMyFiles_Simple) {
  WriteFile("cat.txt", "I like cats");
  WriteFile("dog.txt", "dog text");
  WriteFile("turtle.txt", "turtle text");
  auto quit_closure = task_environment_.QuitClosure();
  RunUntilIdle();

  std::string path1 = Path("cat.txt").MaybeAsASCII();
  std::string path2 = Path("dog.txt").MaybeAsASCII();
  std::string path3 = Path("turtle.txt").MaybeAsASCII();

  // Simple test without bytes and with all files allowed.
  GetSparkyDelegateImpl()->GetMyFiles(
      base::BindLambdaForTesting([&quit_closure, path1, path2, path3](
                                     std::vector<manta::FileData> files_data) {
        EXPECT_EQ((int)files_data.size(), 3);
        EXPECT_THAT(
            files_data,
            UnorderedElementsAre(File(path1, "cat.txt"), File(path2, "dog.txt"),
                                 File(path3, "turtle.txt")));
        for (manta::FileData file : files_data) {
          EXPECT_FALSE(file.bytes.has_value());
        }
        quit_closure.Run();
      }),
      false, std::set<std::string>());

  task_environment_.RunUntilQuit();
}

TEST_F(SparkyDelegateImplTest, GetMyFiles_WithFiler) {
  WriteFile("cat.txt", "I like cats");
  WriteFile("dog.txt", "dog text");
  WriteFile("turtle.txt", "turtle text");
  auto quit_closure = task_environment_.QuitClosure();
  RunUntilIdle();

  std::string path1 = Path("cat.txt").MaybeAsASCII();
  std::string path2 = Path("dog.txt").MaybeAsASCII();
  std::string path3 = Path("turtle.txt").MaybeAsASCII();

  GetSparkyDelegateImpl()->GetMyFiles(
      base::BindLambdaForTesting([&quit_closure, path1, path3](
                                     std::vector<manta::FileData> files_data) {
        EXPECT_EQ((int)files_data.size(), 2);
        EXPECT_THAT(files_data,
                    UnorderedElementsAre(File(path1, "cat.txt"),
                                         File(path3, "turtle.txt")));
        quit_closure.Run();
      }),
      false, std::set<std::string>({path1, path3}));

  task_environment_.RunUntilQuit();
}

TEST_F(SparkyDelegateImplTest, GetMyFiles_WithBytes) {
  WriteFile("cat.txt", "I like cats");
  WriteFile("dog.txt", "dog text");
  WriteFile("turtle.txt", "turtle text");
  auto quit_closure = task_environment_.QuitClosure();
  RunUntilIdle();

  std::string path1 = Path("cat.txt").MaybeAsASCII();
  std::string path2 = Path("dog.txt").MaybeAsASCII();
  std::string path3 = Path("turtle.txt").MaybeAsASCII();

  GetSparkyDelegateImpl()->GetMyFiles(
      base::BindLambdaForTesting([&quit_closure, path1, path2, path3](
                                     std::vector<manta::FileData> files_data) {
        EXPECT_EQ((int)files_data.size(), 3);
        EXPECT_THAT(
            files_data,
            UnorderedElementsAre(File(path1, "cat.txt"), File(path2, "dog.txt"),
                                 File(path3, "turtle.txt")));
        for (manta::FileData file : files_data) {
          EXPECT_TRUE(file.bytes.has_value());
        }
        quit_closure.Run();
      }),
      true, std::set<std::string>());

  task_environment_.RunUntilQuit();
}

TEST_F(SparkyDelegateImplTest, FilesSummary) {
  // If no summary data has yet be inserted, then the requested file vector
  // should be empty.
  std::vector<manta::FileData> empty_files_summary =
      GetSparkyDelegateImpl()->GetFileSummaries();
  EXPECT_TRUE(empty_files_summary.empty());

  std::vector<manta::FileData> files_data;
  auto file_1 = manta::FileData("path1", "name1.pdf", "2024");
  file_1.summary = "file 1 summary";
  file_1.size_in_bytes = (int64_t)8234;
  files_data.emplace_back(file_1);

  auto file_2 = manta::FileData("path2", "name2", "2023");
  file_2.summary = "my second file";
  file_2.size_in_bytes = (int64_t)1287;
  files_data.emplace_back(file_2);

  GetSparkyDelegateImpl()->UpdateFileSummaries(files_data);
  std::vector<manta::FileData> files_summary =
      GetSparkyDelegateImpl()->GetFileSummaries();
  EXPECT_EQ(2, (int)files_summary.size());

  EXPECT_THAT(files_summary,
              UnorderedElementsAre(
                  FileWithSummary("path1", "name1.pdf", "file 1 summary",
                                  "2024", (int64_t)8234),
                  FileWithSummary("path2", "name2", "my second file", "2023",
                                  (int64_t)1287)));

  std::vector<manta::FileData> files_data_updated;
  auto file_2_updated = manta::FileData("path2", "name2", "2024");
  file_2_updated.summary = "my second file with extra cool stuff";
  file_2_updated.size_in_bytes = (int64_t)1987;
  files_data_updated.emplace_back(file_2_updated);

  auto file_3 =
      manta::FileData("my/last/path/tree.png", "tree.png", "yesterday");
  file_3.summary = "a photo of a eucalyptus tree";
  file_3.size_in_bytes = (int64_t)456243;
  files_data_updated.emplace_back(file_3);

  GetSparkyDelegateImpl()->UpdateFileSummaries(files_data_updated);
  std::vector<manta::FileData> files_summary_updated =
      GetSparkyDelegateImpl()->GetFileSummaries();

  EXPECT_EQ(3, (int)files_summary_updated.size());

  EXPECT_THAT(files_summary_updated,
              UnorderedElementsAre(
                  FileWithSummary("path1", "name1.pdf", "file 1 summary",
                                  "2024", (int64_t)8234),
                  FileWithSummary("path2", "name2",
                                  "my second file with extra cool stuff",
                                  "2024", (int64_t)1987),
                  FileWithSummary("my/last/path/tree.png", "tree.png",
                                  "a photo of a eucalyptus tree", "yesterday",
                                  (int64_t)456243)));
}

}  // namespace ash
