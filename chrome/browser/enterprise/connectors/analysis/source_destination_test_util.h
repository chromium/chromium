// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_SOURCE_DESTINATION_TEST_UTIL_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_SOURCE_DESTINATION_TEST_UTIL_H_

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/guest_os/public/types.h"

namespace content {
class BrowserContext;
}

namespace enterprise_connectors {

// A helper class for tests using an analysis connector that supports filtering
// based on volumes using `SourceDestinationMatcherAsh`.
// The constructor will create one directory for each `VolumeInfo` in `volumes`
// and register each directory with the `VolumeManager` associated to `profile`.
// FileSystemURLs on these volumes can be retrieved using
// `GetTestFileSystemURLForVolume()`.
// Use `GetTempDirPath()` to get the parent directory for all volumes.
// `GetTempDirPath()` should also be used to register a `FileSystemContext`.
//
// Registering volumes is necessary for SourceDestinationMatcherAsh to match the
// correct volumes.
class SourceDestinationTestingHelper {
 public:
  struct VolumeInfo {
    file_manager::VolumeType type;
    std::optional<guest_os::VmType> vm_type;
    const char* fs_config_string;
  };

  SourceDestinationTestingHelper(content::BrowserContext* profile,
                                 std::vector<VolumeInfo> volumes);

  // Note that before the destructor is called, the used testing profile should
  // be deleted to ensure that the VolumeManager is removed before the
  // DiskMountManager is shutdown.
  ~SourceDestinationTestingHelper();

  // Get a FileSystemURL on a volume matching `volume_info`.
  // The path `component` is appended to the base directory of the volume.
  // `component` must be a relative path
  storage::FileSystemURL GetTestFileSystemURLForVolume(
      VolumeInfo volume_info,
      const std::string& component = "test.txt");

  base::FilePath GetTempDirPath();

 private:
  void AddVolumes(content::BrowserContext* profile,
                  std::vector<VolumeInfo> volumes);

  base::ScopedTempDir temp_dir_;
  const blink::StorageKey kTestStorageKey =
      blink::StorageKey::CreateFromStringForTesting("chrome://abc");
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_SOURCE_DESTINATION_TEST_UTIL_H_
