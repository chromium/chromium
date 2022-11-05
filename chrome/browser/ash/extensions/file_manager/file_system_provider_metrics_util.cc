// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/file_manager/file_system_provider_metrics_util.h"

#include "base/no_destructor.h"

namespace file_manager {

// Enumeration of known File System Providers used to map to a UMA enumeration
// index. All File System Providers NOT present in this list will be reported as
// UNKNOWN. These look like extension ids, but are actually provider ids which
// may but don't have to be extension ids.
std::unordered_map<std::string, FileSystemProviderMountedType>
GetUmaForFileSystemProvider() {
  static const base::NoDestructor<
      std::unordered_map<std::string, FileSystemProviderMountedType>>
      provider_id_to_uma_sample_map({
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

  return *provider_id_to_uma_sample_map;
}

}  // namespace file_manager
