// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/zxcvbn_data_component_installer.h"

#include <stdint.h>

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_split.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "base/version.h"
#include "components/component_updater/component_installer.h"
#include "components/component_updater/component_updater_service.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"
#include "components/update_client/utils.h"
#include "third_party/zxcvbn-cpp/native-src/zxcvbn/frequency_lists.hpp"
#include "third_party/zxcvbn-cpp/native-src/zxcvbn/frequency_lists_common.hpp"

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

namespace {

// Small struct linking a dictionary tag with the corresponding filename.
struct TagAndFileName {
  zxcvbn::DictionaryTag tag;
  base::FilePath::StringPieceType file_name;
};

constexpr std::array<TagAndFileName, 6> kTagAndFileNamePairs = {{
    {zxcvbn::DictionaryTag::ENGLISH_WIKIPEDIA,
     ZxcvbnDataComponentInstallerPolicy::kEnglishWikipediaTxtFileName},
    {zxcvbn::DictionaryTag::FEMALE_NAMES,
     ZxcvbnDataComponentInstallerPolicy::kFemaleNamesTxtFileName},
    {zxcvbn::DictionaryTag::MALE_NAMES,
     ZxcvbnDataComponentInstallerPolicy::kMaleNamesTxtFileName},
    {zxcvbn::DictionaryTag::PASSWORDS,
     ZxcvbnDataComponentInstallerPolicy::kPasswordsTxtFileName},
    {zxcvbn::DictionaryTag::SURNAMES,
     ZxcvbnDataComponentInstallerPolicy::kSurnamesTxtFileName},
    {zxcvbn::DictionaryTag::US_TV_AND_FILM,
     ZxcvbnDataComponentInstallerPolicy::kUsTvAndFilmTxtFileName},
}};

using RankedDictionaries =
    base::flat_map<zxcvbn::DictionaryTag, zxcvbn::RankedDict>;
RankedDictionaries ParseRankedDictionaries(const base::FilePath& install_dir) {
  RankedDictionaries result;
  for (const auto& pair : kTagAndFileNamePairs) {
    base::FilePath dictionary_path = install_dir.Append(pair.file_name);
    DVLOG(1) << "Reading Dictionary from file: " << dictionary_path;

    std::string dictionary;
    if (base::ReadFileToString(dictionary_path, &dictionary)) {
      result.emplace(pair.tag, zxcvbn::build_ranked_dict(base::SplitStringPiece(
                                   dictionary, "\r\n", base::TRIM_WHITESPACE,
                                   base::SPLIT_WANT_NONEMPTY)));
    } else {
      VLOG(1) << "Failed reading from " << dictionary_path;
    }
  }

  return result;
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
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) const {
  return base::ranges::all_of(kTagAndFileNamePairs, [&](const auto& pair) {
    return base::PathExists(install_dir.Append(pair.file_name));
  });
}

bool ZxcvbnDataComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return false;
}

bool ZxcvbnDataComponentInstallerPolicy::RequiresNetworkEncryption() const {
  return false;
}

update_client::CrxInstaller::Result
ZxcvbnDataComponentInstallerPolicy::OnCustomInstall(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(update_client::InstallError::NONE);
}

void ZxcvbnDataComponentInstallerPolicy::OnCustomUninstall() {}

void ZxcvbnDataComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    std::unique_ptr<base::DictionaryValue> manifest) {
  DVLOG(1) << "Zxcvbn Data Component ready, version " << version.GetString()
           << " in " << install_dir;

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&ParseRankedDictionaries, install_dir),
      base::BindOnce(&zxcvbn::SetRankedDicts));
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

std::vector<std::string> ZxcvbnDataComponentInstallerPolicy::GetMimeTypes()
    const {
  return std::vector<std::string>();
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
