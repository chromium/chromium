// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/source_destination_matcher_ash.h"

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/file_manager/volume_manager_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/disks/disk_mount_manager.h"
#include "chromeos/ash/components/disks/fake_disk_mount_manager.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/common/file_system/file_system_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

namespace {

struct VolumeInfo {
  file_manager::VolumeType type;
  std::optional<guest_os::VmType> vm_type;
  const char* fs_config_string;
};

base::FilePath GetBasePathForVolume(base::FilePath path,
                                    const VolumeInfo& volume_info) {
  base::FilePath volume_path =
      path.Append(base::NumberToString(volume_info.type));
  if (volume_info.vm_type.has_value())
    volume_path = volume_path.Append(
        "_" + base::NumberToString(volume_info.vm_type.value()));
  else
    volume_path = volume_path.Append("_noVmType");

  return volume_path;
}

constexpr std::array kVolumeInfos{
    VolumeInfo{file_manager::VOLUME_TYPE_TESTING, std::nullopt, "TESTING"},
    VolumeInfo{file_manager::VOLUME_TYPE_GOOGLE_DRIVE, std::nullopt,
               "GOOGLE_DRIVE"},
    VolumeInfo{file_manager::VOLUME_TYPE_DOWNLOADS_DIRECTORY, std::nullopt,
               "MY_FILES"},
    VolumeInfo{file_manager::VOLUME_TYPE_REMOVABLE_DISK_PARTITION, std::nullopt,
               "REMOVABLE"},
    VolumeInfo{file_manager::VOLUME_TYPE_MOUNTED_ARCHIVE_FILE, std::nullopt,
               "TESTING"},
    VolumeInfo{file_manager::VOLUME_TYPE_PROVIDED, std::nullopt, "PROVIDED"},
    VolumeInfo{file_manager::VOLUME_TYPE_MTP, std::nullopt,
               "DEVICE_MEDIA_STORAGE"},
    VolumeInfo{file_manager::VOLUME_TYPE_MEDIA_VIEW, std::nullopt, "ARC"},
    VolumeInfo{file_manager::VOLUME_TYPE_CROSTINI, std::nullopt, "CROSTINI"},
    VolumeInfo{file_manager::VOLUME_TYPE_ANDROID_FILES, std::nullopt, "ARC"},
    VolumeInfo{file_manager::VOLUME_TYPE_DOCUMENTS_PROVIDER, std::nullopt,
               "ARC"},
    VolumeInfo{file_manager::VOLUME_TYPE_SMB, std::nullopt, "SMB"},
    VolumeInfo{file_manager::VOLUME_TYPE_SYSTEM_INTERNAL, std::nullopt,
               "UNKNOWN"},
    VolumeInfo{file_manager::VOLUME_TYPE_GUEST_OS, guest_os::VmType::TERMINA,
               "CROSTINI"},
    VolumeInfo{file_manager::VOLUME_TYPE_GUEST_OS, guest_os::VmType::PLUGIN_VM,
               "PLUGIN_VM"},
    VolumeInfo{file_manager::VOLUME_TYPE_GUEST_OS, guest_os::VmType::BOREALIS,
               "BOREALIS"},
    VolumeInfo{file_manager::VOLUME_TYPE_GUEST_OS, guest_os::VmType::BRUSCHETTA,
               "BRUSCHETTA"},
    VolumeInfo{file_manager::VOLUME_TYPE_GUEST_OS, guest_os::VmType::UNKNOWN,
               "UNKNOWN_VM"},
    VolumeInfo{file_manager::VOLUME_TYPE_GUEST_OS, std::nullopt, "UNKNOWN_VM"},
    VolumeInfo{file_manager::VOLUME_TYPE_GUEST_OS, guest_os::VmType::ARCVM,
               "ARC"}};

void AddVolumes(Profile* profile, base::FilePath path) {
  file_manager::VolumeManager* const volume_manager =
      file_manager::VolumeManager::Get(profile);
  for (const VolumeInfo& volume_info : kVolumeInfos) {
    base::FilePath volume_path = GetBasePathForVolume(path, volume_info);
    EXPECT_TRUE(base::CreateDirectory(volume_path));

    if (volume_info.type == file_manager::VOLUME_TYPE_MOUNTED_ARCHIVE_FILE) {
      // A mounted archive needs a proper source path to be mounted correctly.
      base::FilePath source_path =
          GetBasePathForVolume(path, kVolumeInfos[0]).Append("source.zip");
      volume_manager->AddVolumeForTesting(
          file_manager::Volume::CreateForTesting(
              volume_path, volume_info.type, volume_info.vm_type, source_path));
    } else {
      volume_manager->AddVolumeForTesting(
          file_manager::Volume::CreateForTesting(volume_path, volume_info.type,
                                                 volume_info.vm_type));
    }
  }
}

class BaseTest : public testing::Test {
 public:
  BaseTest() : profile_manager_(TestingBrowserProcess::GetGlobal()) {
    EXPECT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("test-user");

    file_manager::VolumeManagerFactory::GetInstance()->SetTestingFactory(
        profile_.get(),
        base::BindLambdaForTesting([](content::BrowserContext* context) {
          return std::unique_ptr<KeyedService>(
              std::make_unique<file_manager::VolumeManager>(
                  Profile::FromBrowserContext(context), nullptr, nullptr,
                  ash::disks::DiskMountManager::GetInstance(), nullptr,
                  file_manager::VolumeManager::GetMtpStorageInfoCallback()));
        }));

    // Takes ownership of `disk_mount_manager_`, but Shutdown() must be called.
    ash::disks::DiskMountManager::InitializeForTesting(
        new ash::disks::FakeDiskMountManager);

    // Register volumes.
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    AddVolumes(profile_, temp_dir_.GetPath());
  }

  ~BaseTest() override {
    profile_manager_.DeleteAllTestingProfiles();
    ash::disks::DiskMountManager::Shutdown();
  }

  storage::FileSystemURL PathToFileSystemURL(base::FilePath path) {
    return storage::FileSystemURL::CreateForTest(
        kTestStorageKey, storage::kFileSystemTypeLocal, path);
  }

  storage::FileSystemURL GetBaseFileSystemURLForVolume(VolumeInfo volume_info) {
    return PathToFileSystemURL(
        GetBasePathForVolume(temp_dir_.GetPath(), volume_info));
  }

  Profile* profile() { return profile_; }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile, DanglingUntriaged> profile_;
  base::ScopedTempDir temp_dir_;
  const blink::StorageKey kTestStorageKey =
      blink::StorageKey::CreateFromStringForTesting("chrome://abc");
};

struct TestParam {
  TestParam(const char* name, const char* settings_value, size_t expected_id)
      : name(name), settings_value(settings_value), expected_id(expected_id) {}

  const char* name;
  const char* settings_value;
  size_t expected_id;
};

}  // namespace

using SourceDestinationMatcherAshTest = BaseTest;

TEST_F(SourceDestinationMatcherAshTest, NullptrSettingsNoCrash) {
  SourceDestinationMatcherAsh matcher;

  size_t id = 0;
  base::Value::List* settings = nullptr;
  matcher.AddFilters(&id, settings);
  EXPECT_EQ(id, 0u);
}

class SourceDestinationMatcherAshAddFilters
    : public BaseTest,
      public testing::WithParamInterface<TestParam> {};

TEST_P(SourceDestinationMatcherAshAddFilters, Test) {
  SourceDestinationMatcherAsh matcher;

  size_t id = 0;
  auto settings = base::JSONReader::Read(GetParam().settings_value,
                                         base::JSON_ALLOW_TRAILING_COMMAS);
  ASSERT_TRUE(settings.has_value());
  auto* settings_list = settings.value().GetIfList();
  ASSERT_TRUE(settings_list);
  matcher.AddFilters(&id, settings_list);
  EXPECT_EQ(id, GetParam().expected_id);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    SourceDestinationMatcherAshAddFilters,
    testing::Values(
        // Validate that the enabled patterns match the expected patterns.
        TestParam("EmptySettings", R"([])", 0),
        TestParam("ListEntryIsNotADict", R"(["blub"])", 0),
        TestParam("ListEntryMissesSourceOrDestination",
                  R"([{
                      "sources": [""],
                    }])",
                  0),
        TestParam("ListEntrySourceOrDestinationNotADict",
                  R"([{
                      "sources": [{"file_system_type":"ANY"}],
                      "destinations": ["file_system_type"],
                    }])",
                  0),
        TestParam("SourceOrDestinationListEntriesEmpty",
                  R"([{
                      "sources": [{"file_system_type":"ANY"}],
                      "destinations": [{}],
                    }])",
                  0),
        TestParam("SourceOrDestinationListEntriesAreInvalid",
                  R"([{
                      "sources": [{"file_system_type":"ANY"}],
                      "destinations": [
                        {
                          "file_system_type":"ANY",
                          "unknown_key": "",
                        },
                      ],
                    }])",
                  0),
        TestParam("FileSystemTypeInvalid",
                  R"([{
                      "sources": [{"file_system_type":"ANY"}],
                      "destinations": [{"file_system_type":"BAD"}],
                    }])",
                  0),
        TestParam("Valid",
                  R"([{
                      "sources": [{"file_system_type":"ANY"}],
                      "destinations": [{"file_system_type":"ANY"}],
                    }])",
                  1),
        TestParam("ValidOneDestinationBad",
                  R"([{
                      "sources": [{"file_system_type":"ANY"}],
                      "destinations": [
                        {"file_system_type":"ANY"},
                        {"file_system_type":"BAD"},
                      ],
                    }])",
                  1)),
    [](const testing::TestParamInfo<TestParam>& info) {
      return info.param.name;
    });

class SourceDestinationMatcherAshParamTest
    : public BaseTest,
      public testing::WithParamInterface<VolumeInfo> {};

TEST_P(SourceDestinationMatcherAshParamTest, FromOneToAny) {
  VolumeInfo source_volume = GetParam();

  SourceDestinationMatcherAsh matcher;

  size_t id = 0;
  auto settings =
      base::JSONReader::Read(base::StringPrintf(R"([
          {
            "sources": [{"file_system_type":"%s"}],
            "destinations": [{"file_system_type":"ANY"}],
          }
        ])",
                                                source_volume.fs_config_string),
                             base::JSON_ALLOW_TRAILING_COMMAS);
  ASSERT_TRUE(settings.has_value());
  auto* settings_list = settings.value().GetIfList();
  ASSERT_TRUE(settings_list);
  matcher.AddFilters(&id, settings_list);
  // One rule should be added!
  EXPECT_EQ(id, 1u);

  for (auto src_test_info : kVolumeInfos) {
    bool should_match = std::string(src_test_info.fs_config_string) ==
                        std::string(source_volume.fs_config_string);
    for (auto dest_test_info : kVolumeInfos) {
      auto matches =
          matcher.Match(profile(), GetBaseFileSystemURLForVolume(src_test_info),
                        GetBaseFileSystemURLForVolume(dest_test_info));
      if (should_match) {
        ASSERT_EQ(matches.size(), 1u)
            << "matches: " << matches.size()
            << ", source: " << src_test_info.fs_config_string
            << ", destination: " << dest_test_info.fs_config_string;
        EXPECT_THAT(matches, testing::ElementsAre(1ul))
            << "source: " << src_test_info.fs_config_string
            << ", destination: " << dest_test_info.fs_config_string;
      } else {
        EXPECT_TRUE(matches.empty())
            << "matches: " << matches.size()
            << ", source: " << src_test_info.fs_config_string
            << ", destination: " << dest_test_info.fs_config_string;
      }
    }
  }
}

TEST_P(SourceDestinationMatcherAshParamTest, FromAnyToOne) {
  VolumeInfo destination_volume = GetParam();

  SourceDestinationMatcherAsh matcher;

  size_t id = 0;
  auto settings = base::JSONReader::Read(
      base::StringPrintf(R"([
          {
            "sources": [{"file_system_type":"ANY"}],
            "destinations": [{"file_system_type":"%s"}],
          }
        ])",
                         destination_volume.fs_config_string),
      base::JSON_ALLOW_TRAILING_COMMAS);
  ASSERT_TRUE(settings.has_value());
  auto* settings_list = settings.value().GetIfList();
  ASSERT_TRUE(settings_list);
  matcher.AddFilters(&id, settings_list);
  // One rule should be added!
  EXPECT_EQ(id, 1u);

  for (auto src_test_info : kVolumeInfos) {
    for (auto dest_test_info : kVolumeInfos) {
      bool should_match = std::string(dest_test_info.fs_config_string) ==
                          std::string(destination_volume.fs_config_string);
      auto matches =
          matcher.Match(profile(), GetBaseFileSystemURLForVolume(src_test_info),
                        GetBaseFileSystemURLForVolume(dest_test_info));
      if (should_match) {
        ASSERT_EQ(matches.size(), 1u)
            << "matches: " << matches.size()
            << ", source: " << src_test_info.fs_config_string
            << ", destination: " << dest_test_info.fs_config_string;
        EXPECT_THAT(matches, testing::ElementsAre(1ul))
            << "source: " << src_test_info.fs_config_string
            << ", destination: " << dest_test_info.fs_config_string;
      } else {
        EXPECT_TRUE(matches.empty())
            << "matches: " << matches.size()
            << ", source: " << src_test_info.fs_config_string
            << ", destination: " << dest_test_info.fs_config_string;
      }
    }
  }
}

INSTANTIATE_TEST_SUITE_P(,
                         SourceDestinationMatcherAshParamTest,
                         testing::ValuesIn(kVolumeInfos));

TEST(SourceDestinationMatcherAshFsTypeStringConversionTest, StringToType) {
  std::vector<std::pair<std::string, SourceDestinationMatcherAsh::FsType>>
      pairs{
          {"TESTING", SourceDestinationMatcherAsh::FsType::kTesting},
          {"UNKNOWN", SourceDestinationMatcherAsh::FsType::kUnknown},
          {"ANY", SourceDestinationMatcherAsh::FsType::kAny},
          {"*", SourceDestinationMatcherAsh::FsType::kAny},
          {"MY_FILES", SourceDestinationMatcherAsh::FsType::kMyFiles},
          {"REMOVABLE", SourceDestinationMatcherAsh::FsType::kRemovable},
          {"DEVICE_MEDIA_STORAGE",
           SourceDestinationMatcherAsh::FsType::kDeviceMediaStorage},
          {"PROVIDED", SourceDestinationMatcherAsh::FsType::kProvided},
          {"ARC", SourceDestinationMatcherAsh::FsType::kArc},
          {"GOOGLE_DRIVE", SourceDestinationMatcherAsh::FsType::kGoogleDrive},
          {"SMB", SourceDestinationMatcherAsh::FsType::kSmb},
          {"CROSTINI", SourceDestinationMatcherAsh::FsType::kCrostini},
          {"PLUGIN_VM", SourceDestinationMatcherAsh::FsType::kPluginVm},
          {"BOREALIS", SourceDestinationMatcherAsh::FsType::kBorealis},
          {"BRUSCHETTA", SourceDestinationMatcherAsh::FsType::kBruschetta},
          {"UNKNOWN_VM", SourceDestinationMatcherAsh::FsType::kUnknownVm},
      };
  for (auto [fs_string, expected_fs_type] : pairs) {
    EXPECT_EQ(expected_fs_type,
              SourceDestinationMatcherAsh::StringToFsType(fs_string));
  }
}

TEST(SourceDestinationMatcherAshFsTypeStringConversionTest, TypeToString) {
  std::vector<std::pair<SourceDestinationMatcherAsh::FsType, std::string>>
      pairs{
          {SourceDestinationMatcherAsh::FsType::kTesting, "TESTING"},
          {SourceDestinationMatcherAsh::FsType::kUnknown, "UNKNOWN"},
          {SourceDestinationMatcherAsh::FsType::kAny, "ANY"},
          {SourceDestinationMatcherAsh::FsType::kMyFiles, "MY_FILES"},
          {SourceDestinationMatcherAsh::FsType::kRemovable, "REMOVABLE"},
          {SourceDestinationMatcherAsh::FsType::kDeviceMediaStorage,
           "DEVICE_MEDIA_STORAGE"},
          {SourceDestinationMatcherAsh::FsType::kProvided, "PROVIDED"},
          {SourceDestinationMatcherAsh::FsType::kArc, "ARC"},
          {SourceDestinationMatcherAsh::FsType::kGoogleDrive, "GOOGLE_DRIVE"},
          {SourceDestinationMatcherAsh::FsType::kSmb, "SMB"},
          {SourceDestinationMatcherAsh::FsType::kCrostini, "CROSTINI"},
          {SourceDestinationMatcherAsh::FsType::kPluginVm, "PLUGIN_VM"},
          {SourceDestinationMatcherAsh::FsType::kBorealis, "BOREALIS"},
          {SourceDestinationMatcherAsh::FsType::kBruschetta, "BRUSCHETTA"},
          {SourceDestinationMatcherAsh::FsType::kUnknownVm, "UNKNOWN_VM"},
      };

  for (auto [fs_type, expected_fs_string] : pairs) {
    EXPECT_EQ(expected_fs_string,
              SourceDestinationMatcherAsh::FsTypeToString(fs_type));
  }
}

}  // namespace enterprise_connectors
