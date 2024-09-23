// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_OPENED_CLOUD_FILE_H_
#define CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_OPENED_CLOUD_FILE_H_

#include "base/files/file_path.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_interface.h"

namespace ash::file_system_provider {

// Contains information about an opened file that is either being retrieved from
// an FSP and then streaming to the content cache OR is being directly served
// from the content cache.
struct OpenedCloudFile {
  OpenedCloudFile(const base::FilePath& file_path,
                  OpenFileMode mode,
                  int request_id,
                  const std::string& version_tag,
                  std::optional<int64_t> bytes_in_cloud);
  ~OpenedCloudFile();

  // The absolute path of the file that is rooted at the FSP.
  // e.g. /Documents/test.txt
  base::FilePath file_path;

  // The mode the file was opened with, currently write mode is not supported.
  OpenFileMode mode;

  // The request ID that this opened file is associated with with the underlying
  // FSP.
  int request_id = -1;

  // The version tag for the opened file (as retrieved via the metadata sent
  // back from OpenFile). This is used to compare against the version tag in the
  // content cache.
  std::string version_tag;

  // The expected bytes that exist in the cloud for this file.
  std::optional<int64_t> bytes_in_cloud;
};

}  // namespace ash::file_system_provider

#endif  // CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_OPENED_CLOUD_FILE_H_
