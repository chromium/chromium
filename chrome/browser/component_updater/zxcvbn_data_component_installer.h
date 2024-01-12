// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_ZXCVBN_DATA_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_ZXCVBN_DATA_COMPONENT_INSTALLER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/values.h"
#include "base/version.h"
#include "components/component_updater/component_installer.h"
#include "components/component_updater/component_updater_service.h"
#include "components/update_client/update_client.h"
#include "components/update_client/utils.h"

namespace component_updater {

class ZxcvbnDataComponentInstallerPolicy : public ComponentInstallerPolicy {
 public:
  // The filenames of the word lists in text format.
  static constexpr base::FilePath::StringPieceType
      kEnglishWikipediaTxtFileName = FILE_PATH_LITERAL("english_wikipedia.txt");
  static constexpr base::FilePath::StringPieceType kFemaleNamesTxtFileName =
      FILE_PATH_LITERAL("female_names.txt");
  static constexpr base::FilePath::StringPieceType kMaleNamesTxtFileName =
      FILE_PATH_LITERAL("male_names.txt");
  static constexpr base::FilePath::StringPieceType kPasswordsTxtFileName =
      FILE_PATH_LITERAL("passwords.txt");
  static constexpr base::FilePath::StringPieceType kSurnamesTxtFileName =
      FILE_PATH_LITERAL("surnames.txt");
  static constexpr base::FilePath::StringPieceType kUsTvAndFilmTxtFileName =
      FILE_PATH_LITERAL("us_tv_and_film.txt");

  static constexpr std::array<base::FilePath::StringPieceType, 6> kFileNames = {
      {
          kEnglishWikipediaTxtFileName,
          kFemaleNamesTxtFileName,
          kMaleNamesTxtFileName,
          kPasswordsTxtFileName,
          kSurnamesTxtFileName,
          kUsTvAndFilmTxtFileName,
      }};

  // The filename of the combined word list in the format that
  // `zxcvbn::RankedDicts` uses internally.
  static constexpr base::FilePath::StringPieceType
      kCombinedRankedDictsFileName = FILE_PATH_LITERAL("ranked_dicts");

  // ComponentInstallerPolicy overrides:
  // Confirms that the version entry in the manifest exists and is well-formed
  // and verifies that all files expected for the component version exist.
  bool VerifyInstallation(const base::Value::Dict& manifest,
                          const base::FilePath& install_dir) const override;

  bool SupportsGroupPolicyEnabledComponentUpdates() const override;

  bool RequiresNetworkEncryption() const override;

  update_client::CrxInstaller::Result OnCustomInstall(
      const base::Value::Dict& manifest,
      const base::FilePath& install_dir) override;

  void OnCustomUninstall() override;

  void ComponentReady(const base::Version& version,
                      const base::FilePath& install_dir,
                      base::Value::Dict manifest) override;

  base::FilePath GetRelativeInstallDir() const override;

  void GetHash(std::vector<uint8_t>* hash) const override;

  std::string GetName() const override;

  update_client::InstallerAttributes GetInstallerAttributes() const override;
};

// Call once during startup to make the component update service aware of
// the zxcvbn data component.
void RegisterZxcvbnDataComponent(ComponentUpdateService* cus);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_ZXCVBN_DATA_COMPONENT_INSTALLER_H_
