// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_SOURCE_DESTINATION_TEST_UTIL_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_SOURCE_DESTINATION_TEST_UTIL_H_

#include <string>
#include <utility>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/guest_os/public/types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {
class BrowserContext;
}

namespace enterprise_connectors {

class SourceDestinationTestingHelper {
 public:
  struct VolumeInfo {
    file_manager::VolumeType type;
    absl::optional<guest_os::VmType> vm_type;
    const char* fs_config_string;
  };

  SourceDestinationTestingHelper(content::BrowserContext* profile,
                                 std::vector<VolumeInfo> volumes);

  // Note that before the destructor is called, the used testing profile should
  // be deleted to ensure that the VolumeManager is removed before the
  // DiskMountManager is shutdown.
  ~SourceDestinationTestingHelper();

  storage::FileSystemURL GetTestFileSystemURLForVolume(VolumeInfo volume_info);

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
