// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/opened_cloud_file.h"

namespace ash::file_system_provider {

OpenedCloudFile::OpenedCloudFile(const base::FilePath& file_path,
                                 OpenFileMode mode,
                                 int request_id,
                                 const std::string& version_tag,
                                 std::optional<int64_t> bytes_in_cloud)
    : file_path(file_path),
      mode(mode),
      request_id(request_id),
      version_tag(version_tag),
      bytes_in_cloud(bytes_in_cloud) {}

OpenedCloudFile::~OpenedCloudFile() = default;

}  // namespace ash::file_system_provider
