// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/zxcvbn_data_component_installer.h"

#include <array>
#include <bit>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_split.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "base/version.h"
#include "components/component_updater/component_installer.h"
#include "components/component_updater/component_updater_service.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"
#include "components/update_client/utils.h"
#include "third_party/zxcvbn-cpp/native-src/zxcvbn/frequency_lists.hpp"

namespace component_updater {

constexpr base::FilePath::StringPieceType
    ZxcvbnDataComponentInstallerPolicy::kEnglishWikipediaTxtFileName;
constexpr base::FilePath::StringPieceType
    ZxcvbnDataComponentInstallerPolicy::kFemaleNamesTxtFileName;
constexpr base::FilePath::StringPieceType
    ZxcvbnDataComponentInstallerPolicy::kMaleNamesTxtFileName;
constexpr base::FilePath::StringPieceType
    ZxcvbnDataComponentInstallerPolicy::kPasswordsTxtFileName;
constexpr base::FilePath::StringPieceType
    ZxcvbnDataComponentInstallerPolicy::kSurnamesTxtFileName;
constexpr base::FilePath::StringPieceType
    ZxcvbnDataComponentInstallerPolicy::kUsTvAndFilmTxtFileName;

constexpr base::FilePath::StringPieceType
    ZxcvbnDataComponentInstallerPolicy::kCombinedRankedDictsFileName;

namespace {

constexpr char kFirstMemoryMappedVersion[] = "2";

// The size (in bytes) of the marker at the beginning of the (memory mapped)
// combined ranked dictionaries file.
constexpr int kNumMarkerBytes = 1;
// The marker bit - see also `zxcvbn::MarkedBigEndianU15::MARKER_BIT`.
constexpr uint8_t kMarkerBit = 0x80;

zxcvbn::RankedDicts MemoryMapRankedDictionaries(
    const base::FilePath& install_dir) {
  base::FilePath dictionary_path = install_dir.Append(
      ZxcvbnDataComponentInstallerPolicy::kCombinedRankedDictsFileName);
  DVLOG(1) << "Memory mapping dictionary from file: " << dictionary_path;
  auto map = std::make_unique<base::MemoryMappedFile>();
  if (!map->Initialize(dictionary_path)) {
    VLOG(1) << "Failed to memory map file from " << dictionary_path;
    return zxcvbn::RankedDicts(nullptr);
  }
  return zxcvbn::RankedDicts(std::move(map));
}

// The SHA256 of the SubjectPublicKeyInfo used to sign the extension.
// The extension id is: ojhpjlocmbogdgmfpkhlaaeamibhnphh
constexpr std::array<uint8_t, 32> kZxcvbnDataPublicKeySha256 = {
    0xe9, 0x7f, 0x9b, 0xe2, 0xc1, 0xe6, 0x36, 0xc5, 0xfa, 0x7b, 0x00,
    0x40, 0xc8, 0x17, 0xdf, 0x77, 0x34, 0x64, 0x84, 0x70, 0x3d, 0xa2,
    0x14, 0x9a, 0x79, 0x99, 0x57, 0xa7, 0x1e, 0x66, 0xf5, 0xb3,
};

}  // namespace

bool ZxcvbnDataComponentInstallerPolicy::VerifyInstallation(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) const {
  const std::string* version_string = manifest.FindString("version");
  if (!version_string) {
    return false;
  }

  base::Version version(*version_string);
  if (!version.IsValid()) {
    return false;
  }

  if (base::ranges::any_of(kFileNames, [&install_dir](const auto& file_name) {
        return !base::PathExists(install_dir.Append(file_name));
      })) {
    return false;
  }

  if (version < base::Version(kFirstMemoryMappedVersion)) {
    return false;
  }

  // If the version supports memory mapping, then the binary file that contains
  // the combined ranked dictionaries must exist, too.
  const base::FilePath combined_ranked_dicts_path = install_dir.Append(
      ZxcvbnDataComponentInstallerPolicy::kCombinedRankedDictsFileName);
  if (!base::PathExists(combined_ranked_dicts_path)) {
    return false;
  }

  // Perform a minimal check that the file has not been corrupted - otherwise
  // the client will run into a failing CHECK when using the library.
  // See (crbug.com/1505352) for instances where this occurred.
  char local_buffer[kNumMarkerBytes] = {};
  if (base::ReadFile(combined_ranked_dicts_path, local_buffer,
                     /*max_size=*/kNumMarkerBytes) != kNumMarkerBytes) {
    return false;
  }
  return std::bit_cast<uint8_t>(local_buffer[0]) & kMarkerBit;
}

bool ZxcvbnDataComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return true;
}

bool ZxcvbnDataComponentInstallerPolicy::RequiresNetworkEncryption() const {
  return false;
}

update_client::CrxInstaller::Result
ZxcvbnDataComponentInstallerPolicy::OnCustomInstall(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(update_client::InstallError::NONE);
}

void ZxcvbnDataComponentInstallerPolicy::OnCustomUninstall() {}

void ZxcvbnDataComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    base::Value::Dict manifest) {
  DVLOG(1) << "Zxcvbn Data Component ready, version " << version.GetString()
           << " in " << install_dir;

  if (version >= base::Version(kFirstMemoryMappedVersion)) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce(&MemoryMapRankedDictionaries, install_dir),
        base::BindOnce(&zxcvbn::SetRankedDicts));
  } else {
    DVLOG(1) << "Zxcvbn Data Component failed, old version";
  }
}

base::FilePath ZxcvbnDataComponentInstallerPolicy::GetRelativeInstallDir()
    const {
  return base::FilePath(FILE_PATH_LITERAL("ZxcvbnData"));
}

void ZxcvbnDataComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign(kZxcvbnDataPublicKeySha256.begin(),
               kZxcvbnDataPublicKeySha256.end());
}

std::string ZxcvbnDataComponentInstallerPolicy::GetName() const {
  return "Zxcvbn Data Dictionaries";
}

update_client::InstallerAttributes
ZxcvbnDataComponentInstallerPolicy::GetInstallerAttributes() const {
  return update_client::InstallerAttributes();
}

void RegisterZxcvbnDataComponent(ComponentUpdateService* cus) {
  base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<ZxcvbnDataComponentInstallerPolicy>())
      ->Register(cus, base::NullCallback());
}

}  // namespace component_updater
