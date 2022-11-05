// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_FILE_SYSTEM_PROVIDER_METRICS_UTIL_H_
#define CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_FILE_SYSTEM_PROVIDER_METRICS_UTIL_H_

#include <string>
#include <unordered_map>
#include <vector>

namespace file_manager {

// UMA metric name that tracks the mounted File System Provider.
inline constexpr char kFileSystemProviderMountedMetricName[] =
    "FileBrowser.FileSystemProviderMounted";

// List of known File System Providers and their corresponding UMA enum value.
enum class FileSystemProviderMountedType {
  UNKNOWN = 0,
  ZIP_UNPACKER = 1,
  FILE_SYSTEM_FOR_DROPBOX = 2,
  FILE_SYSTEM_FOR_ONEDRIVE = 3,
  SFTP_FILE_SYSTEM = 4,
  BOX_FOR_CHROMEOS = 5,
  TED_TALKS = 6,
  WEBDAV_FILE_SYSTEM = 7,
  CLOUD_STORAGE = 8,
  SCAN = 9,
  FILE_SYSTEM_FOR_SMB_CIFS = 10,
  ADD_MY_DOCUMENTS = 11,
  WICKED_GOOD_UNARCHIVER = 12,
  NETWORK_FILE_SHARE_FOR_CHROMEOS = 13,
  LAN_FOLDER = 14,
  ZIP_ARCHIVER = 15,
  SECURE_SHELL_APP = 16,
  NATIVE_NETWORK_SMB = 17,
  kMaxValue = NATIVE_NETWORK_SMB,
};

// Returns a map of File System Provider extension ID to the UMA value that is
// recorded.
std::unordered_map<std::string, FileSystemProviderMountedType>
GetUmaForFileSystemProvider();

}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_FILE_SYSTEM_PROVIDER_METRICS_UTIL_H_
