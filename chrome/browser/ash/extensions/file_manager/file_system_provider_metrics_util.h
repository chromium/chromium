// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_FILE_SYSTEM_PROVIDER_METRICS_UTIL_H_
#define CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_FILE_SYSTEM_PROVIDER_METRICS_UTIL_H_

#include <string_view>

#include "base/containers/fixed_flat_map.h"

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

// Enumeration of known File System Providers used to map to a UMA enumeration
// index. All File System Providers NOT present in this list will be reported as
// UNKNOWN. These look like extension ids, but are actually provider ids which
// may but don't have to be extension ids.
inline constexpr auto kUmaForFileSystemProvider =
    base::MakeFixedFlatMap<std::string_view, FileSystemProviderMountedType>({
        {"hlffpaajmfllggclnjppbblobdhokjhe",
         FileSystemProviderMountedType::FILE_SYSTEM_FOR_DROPBOX},
        {"jbfdfcehgafdbfpniaimfbfomafoadgo",
         FileSystemProviderMountedType::FILE_SYSTEM_FOR_ONEDRIVE},
        {"gbheifiifcfekkamhepkeogobihicgmn",
         FileSystemProviderMountedType::SFTP_FILE_SYSTEM},
        {"dikonaebkejmpbpcnnmfaeopkaenicgf",
         FileSystemProviderMountedType::BOX_FOR_CHROMEOS},
        {"iibcngmpkgghccnakicfmgajlkhnohep",
         FileSystemProviderMountedType::TED_TALKS},
        {"hmckflbfniicjijmdoffagjkpnjgbieh",
         FileSystemProviderMountedType::WEBDAV_FILE_SYSTEM},
        {"ibfbhbegfkamboeglpnianlggahglbfi",
         FileSystemProviderMountedType::CLOUD_STORAGE},
        {"pmnllmkmjilbojkpgplbdmckghmaocjh",
         FileSystemProviderMountedType::SCAN},
        {"mfhnnfciefdpolbelmfkpmhhmlkehbdf",
         FileSystemProviderMountedType::FILE_SYSTEM_FOR_SMB_CIFS},
        {"plmanjiaoflhcilcfdnjeffklbgejmje",
         FileSystemProviderMountedType::ADD_MY_DOCUMENTS},
        {"mljpablpddhocfbnokacjggdbmafjnon",
         FileSystemProviderMountedType::WICKED_GOOD_UNARCHIVER},
        {"ndjpildffkeodjdaeebdhnncfhopkajk",
         FileSystemProviderMountedType::NETWORK_FILE_SHARE_FOR_CHROMEOS},
        {"gmhmnhjihabohahcllfgjooaoecglhpi",
         FileSystemProviderMountedType::LAN_FOLDER},
        {"pnhechapfaindjhompbnflcldabbghjo",
         FileSystemProviderMountedType::SECURE_SHELL_APP},
        {"okddffdblfhhnmhodogpojmfkjmhinfp",
         FileSystemProviderMountedType::SECURE_SHELL_APP},
        {"iodihamcpbpeioajjeobimgagajmlibd",
         FileSystemProviderMountedType::SECURE_SHELL_APP},
        {"algkcnfjnajfhgimadimbjhmpaeohhln",
         FileSystemProviderMountedType::SECURE_SHELL_APP},
        {"@smb", FileSystemProviderMountedType::NATIVE_NETWORK_SMB},
    });

}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_FILE_SYSTEM_PROVIDER_METRICS_UTIL_H_
