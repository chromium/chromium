// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/source_destination_test_util.h"

#include "base/files/file_util.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/file_manager/volume_manager_factory.h"
#include "chromeos/ash/components/disks/disk_mount_manager.h"
#include "chromeos/ash/components/disks/fake_disk_mount_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

namespace {
base::FilePath GetBasePathForVolume(
    base::FilePath path,
    const SourceDestinationTestingHelper::VolumeInfo& volume_info) {
  base::FilePath volume_path =
      path.Append(base::NumberToString(volume_info.type));
  if (volume_info.vm_type.has_value())
    volume_path = volume_path.Append(
        "_" + base::NumberToString(volume_info.vm_type.value()));
  else
    volume_path = volume_path.Append("NoVmType");

  return volume_path;
}

}  // namespace

SourceDestinationTestingHelper::SourceDestinationTestingHelper(
    content::BrowserContext* profile,
    std::vector<VolumeInfo> volumes) {
  file_manager::VolumeManagerFactory::GetInstance()->SetTestingFactory(
      profile, base::BindLambdaForTesting([](content::BrowserContext* context) {
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
  AddVolumes(profile, std::move(volumes));
}

SourceDestinationTestingHelper::~SourceDestinationTestingHelper() {
  ash::disks::DiskMountManager::Shutdown();
}

storage::FileSystemURL
SourceDestinationTestingHelper::GetTestFileSystemURLForVolume(
    VolumeInfo volume_info,
    const std::string& component) {
  return storage::FileSystemURL::CreateForTest(
      kTestStorageKey, storage::kFileSystemTypeLocal,
      GetBasePathForVolume(temp_dir_.GetPath(), volume_info).Append(component));
}

base::FilePath SourceDestinationTestingHelper::GetTempDirPath() {
  return temp_dir_.GetPath();
}

void SourceDestinationTestingHelper::AddVolumes(
    content::BrowserContext* profile,
    std::vector<VolumeInfo> volumes) {
  file_manager::VolumeManager* const volume_manager =
      file_manager::VolumeManager::Get(profile);

  for (const VolumeInfo& volume_info : volumes) {
    base::FilePath volume_path =
        GetBasePathForVolume(temp_dir_.GetPath(), volume_info);
    EXPECT_TRUE(base::CreateDirectory(volume_path));

    if (volume_info.type == file_manager::VOLUME_TYPE_MOUNTED_ARCHIVE_FILE) {
      // A mounted archive needs a proper source path to be mounted correctly.
      base::FilePath source_path =
          GetBasePathForVolume(temp_dir_.GetPath(), volumes[0])
              .Append("source.zip");
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

}  // namespace enterprise_connectors
