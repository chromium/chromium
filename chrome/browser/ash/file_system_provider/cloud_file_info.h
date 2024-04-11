// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_CLOUD_FILE_INFO_H_
#define CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_CLOUD_FILE_INFO_H_

#include <string>

namespace ash::file_system_provider {

// Represents version information relating to a particular file in cloud
// storage.
struct CloudFileInfo {
  std::string version_tag;

  explicit CloudFileInfo(const std::string& version_tag);

  CloudFileInfo(const CloudFileInfo&) = delete;
  CloudFileInfo& operator=(const CloudFileInfo&) = delete;

  ~CloudFileInfo();

  // Enables comparison for unit tests.
  bool operator==(const CloudFileInfo&) const;
};

}  // namespace ash::file_system_provider

#endif  // CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_CLOUD_FILE_INFO_H_
