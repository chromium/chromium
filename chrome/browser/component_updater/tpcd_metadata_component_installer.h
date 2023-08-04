// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_TPCD_METADATA_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_TPCD_METADATA_COMPONENT_INSTALLER_H_

#include <string>

#include "base/functional/callback.h"
#include "base/values.h"
#include "components/component_updater/component_installer.h"
#include "components/update_client/update_client.h"

namespace base {
class FilePath;
}  // namespace base

namespace component_updater {
class TpcdMetadataComponentInstaller : public ComponentInstallerPolicy {
 public:
  using OnTpcdMetadataComponentReadyCallback =
      base::RepeatingCallback<void(std::string)>;

  explicit TpcdMetadataComponentInstaller(
      OnTpcdMetadataComponentReadyCallback on_component_ready_callback);
  ~TpcdMetadataComponentInstaller() override;

  TpcdMetadataComponentInstaller(const TpcdMetadataComponentInstaller&) =
      delete;
  TpcdMetadataComponentInstaller& operator=(
      const TpcdMetadataComponentInstaller&) = delete;

  // Start For testing:
  static void WriteComponentForTesting(const base::FilePath& install_dir,
                                       base::StringPiece contents);
  static void ResetForTesting();
  // End for testing.

 private:
  FRIEND_TEST_ALL_PREFIXES(TpcdMetadataComponentInstallerTest,
                           VerifyAttributes);

  // Start of ComponentInstallerPolicy overrides:
  bool SupportsGroupPolicyEnabledComponentUpdates() const override;
  bool RequiresNetworkEncryption() const override;
  update_client::CrxInstaller::Result OnCustomInstall(
      const base::Value::Dict& manifest,
      const base::FilePath& install_dir) override;
  void OnCustomUninstall() override;
  bool VerifyInstallation(const base::Value::Dict& manifest,
                          const base::FilePath& install_dir) const override;
  void ComponentReady(const base::Version& version,
                      const base::FilePath& install_dir,
                      base::Value::Dict manifest) override;
  base::FilePath GetRelativeInstallDir() const override;
  void GetHash(std::vector<uint8_t>* hash) const override;
  std::string GetName() const override;
  update_client::InstallerAttributes GetInstallerAttributes() const override;
  // End of ComponentInstallerPolicy overrides.

  base::FilePath installed_file_path_;

  static const base::FilePath GetInstalledFilePath(const base::FilePath& base);
  void MaybeFireCallback(
      const absl::optional<std::string>& maybe_classifications);

  OnTpcdMetadataComponentReadyCallback on_component_ready_callback_;
};

// Called once during startup to make the component updater service aware of
// the TPCD Metadata component.
void RegisterTpcdMetadataComponent(ComponentUpdateService* cus);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_TPCD_METADATA_COMPONENT_INSTALLER_H_
