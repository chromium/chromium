// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_SODA_LANGUAGE_PACK_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_SODA_LANGUAGE_PACK_COMPONENT_INSTALLER_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/containers/flat_set.h"
#include "base/optional.h"
#include "chrome/common/pref_names.h"
#include "components/component_updater/component_installer.h"
#include "components/soda/constants.h"
#include "components/update_client/update_client.h"

class PrefService;

namespace base {
class FilePath;
}  // namespace base

namespace component_updater {

// Success callback to be run after the component is downloaded.
using OnSodaLanguagePackComponentInstalledCallback =
    base::RepeatingCallback<void(const base::FilePath&)>;

using OnSodaLanguagePackComponentReadyCallback = base::OnceClosure;

// Describes all metadata needed to dynamically install ChromeOS components.
struct SodaLanguagePackComponentConfig {
  // The language code of the language pack component.
  speech::LanguageCode language_code = speech::LanguageCode::kNone;

  // The language name for the language component (e.g. "en-US").
  const char* language_name = nullptr;

  // The name of the config file path pref for the language pack.
  const char* config_path_pref = nullptr;

  // The SHA256 of the SubjectPublicKeyInfo used to sign the language pack
  // component.
  const uint8_t public_key_sha[32] = {};
};

constexpr SodaLanguagePackComponentConfig kLanguageComponentConfigs[] = {
    {speech::LanguageCode::kEnUs,
     "en-US",
     prefs::kSodaEnUsConfigPath,
     {0xe4, 0x64, 0x1c, 0xc2, 0x8c, 0x2a, 0x97, 0xa7, 0x16, 0x61, 0xbd,
      0xa9, 0xbe, 0xe6, 0x93, 0x56, 0xf5, 0x05, 0x33, 0x9b, 0x8b, 0x0b,
      0x02, 0xe2, 0x6b, 0x7e, 0x6c, 0x40, 0xa1, 0xd2, 0x7e, 0x18}},
    {speech::LanguageCode::kDeDe,
     "de-DE",
     prefs::kSodaDeDeConfigPath,
     {0x92, 0xb6, 0xd8, 0xa3, 0x0b, 0x09, 0xce, 0x21, 0xdb, 0x68, 0x48,
      0x15, 0xcb, 0x49, 0xd7, 0xc6, 0x21, 0x3f, 0xe5, 0x96, 0x10, 0x97,
      0x6e, 0x0f, 0x08, 0x31, 0xec, 0xe4, 0x7f, 0xed, 0xef, 0x3d}},
    {speech::LanguageCode::kEsEs,
     "es-ES",
     prefs::kSodaEsEsConfigPath,
     {0x9a, 0x22, 0xac, 0x04, 0x97, 0xc1, 0x70, 0x61, 0x24, 0x1f, 0x49,
      0x18, 0x72, 0xd8, 0x67, 0x31, 0x72, 0x7a, 0xf9, 0x77, 0x04, 0xf0,
      0x17, 0xb5, 0xfe, 0x88, 0xac, 0x60, 0xdd, 0x8a, 0x67, 0xdd}},
    {speech::LanguageCode::kFrFr,
     "fr-FR",
     prefs::kSodaFrFrConfigPath,
     {0x6e, 0x0e, 0x2b, 0xd3, 0xc6, 0xe5, 0x1b, 0x5e, 0xfa, 0xef, 0x42,
      0x3f, 0x57, 0xb9, 0x2b, 0x13, 0x56, 0x47, 0x58, 0xdb, 0x76, 0x89,
      0x71, 0xeb, 0x1f, 0xed, 0x48, 0x6c, 0xac, 0xd5, 0x31, 0xa0}},
    {speech::LanguageCode::kItIt,
     "it-IT",
     prefs::kSodaItItConfigPath,
     {0x97, 0x45, 0xd7, 0xbc, 0xf0, 0x61, 0x24, 0xb3, 0x0e, 0x13, 0xf2,
      0x97, 0xaa, 0xd5, 0x9e, 0x78, 0xa5, 0x81, 0x35, 0x75, 0xb5, 0x9d,
      0x3b, 0xbb, 0xde, 0xba, 0x0e, 0xf7, 0xf0, 0x48, 0x56, 0x01}},
    {speech::LanguageCode::kJaJp,
     "ja-JP",
     prefs::kSodaJaJpConfigPath,
     {0xed, 0x7f, 0x96, 0xa5, 0x60, 0x9c, 0xaa, 0x4d, 0x80, 0xe5, 0xb8,
      0x26, 0xea, 0xf0, 0x41, 0x50, 0x09, 0x52, 0xa4, 0xb3, 0x1e, 0x6a,
      0x8e, 0x24, 0x99, 0xde, 0x51, 0x14, 0xc4, 0x3c, 0xfa, 0x48}},
};

class SodaLanguagePackComponentInstallerPolicy
    : public ComponentInstallerPolicy {
 public:
  SodaLanguagePackComponentInstallerPolicy(
      SodaLanguagePackComponentConfig language_config,
      OnSodaLanguagePackComponentInstalledCallback on_installed_callback,
      OnSodaLanguagePackComponentReadyCallback on_ready_callback);
  ~SodaLanguagePackComponentInstallerPolicy() override;

  SodaLanguagePackComponentInstallerPolicy(
      const SodaLanguagePackComponentInstallerPolicy&) = delete;
  SodaLanguagePackComponentInstallerPolicy& operator=(
      const SodaLanguagePackComponentInstallerPolicy&) = delete;

  static std::string GetExtensionId(speech::LanguageCode language_code);
  static base::flat_set<std::string> GetExtensionIds();
  static void UpdateSodaLanguagePackComponentOnDemand(
      speech::LanguageCode language_code);
  static base::Optional<SodaLanguagePackComponentConfig>
  GetLanguageComponentConfig(speech::LanguageCode language_code);
  static base::Optional<SodaLanguagePackComponentConfig>
  GetLanguageComponentConfig(const std::string& language_name);

 private:
  FRIEND_TEST_ALL_PREFIXES(SodaLanguagePackComponentInstallerTest,
                           ComponentReady_CallsLambda);

  // The following methods override ComponentInstallerPolicy.
  bool SupportsGroupPolicyEnabledComponentUpdates() const override;
  bool RequiresNetworkEncryption() const override;
  update_client::CrxInstaller::Result OnCustomInstall(
      const base::DictionaryValue& manifest,
      const base::FilePath& install_dir) override;
  void OnCustomUninstall() override;
  bool VerifyInstallation(const base::DictionaryValue& manifest,
                          const base::FilePath& install_dir) const override;
  void ComponentReady(const base::Version& version,
                      const base::FilePath& install_dir,
                      std::unique_ptr<base::DictionaryValue> manifest) override;
  base::FilePath GetRelativeInstallDir() const override;
  void GetHash(std::vector<uint8_t>* hash) const override;
  std::string GetName() const override;
  update_client::InstallerAttributes GetInstallerAttributes() const override;
  std::vector<std::string> GetMimeTypes() const override;

  SodaLanguagePackComponentConfig language_config_;

  OnSodaLanguagePackComponentInstalledCallback on_installed_callback_;
  OnSodaLanguagePackComponentReadyCallback on_ready_callback_;
};

void RegisterSodaLanguagePackComponent(
    SodaLanguagePackComponentConfig language_config,
    ComponentUpdateService* cus,
    PrefService* prefs,
    base::OnceClosure on_ready_callback);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_SODA_LANGUAGE_PACK_COMPONENT_INSTALLER_H_
