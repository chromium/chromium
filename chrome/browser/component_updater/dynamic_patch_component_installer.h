// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_DYNAMIC_PATCH_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_DYNAMIC_PATCH_COMPONENT_INSTALLER_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/values.h"
#include "chrome/browser/component_updater/install_product_component.h"
#include "components/component_updater/component_installer.h"

namespace base {
class Version;
}  // namespace base

namespace component_updater {

class ComponentUpdateService;

// The installer policy for the dynamic patch component, which delivers patched
// child process binaries (e.g., chrome_renderer.dll) for the running version of
// the browser. The component contains an inner CRX, which is installed via
// InstallProductComponent() on behalf of the current user and User Data
// directory into
//   <InstallDir>\<BaseVersion>\ChildModules\<UserPathComponent>\<PatchVersion>
// (i.e., a subdirectory of `chrome::DIR_CHILD_MODULE_ROOT`), where the browser
// finds it and uses it for new child processes.
//
// The version of the component must equal that of its inner CRX: setup.exe
// installs the inner CRX into a directory named for the inner CRX's version,
// and VerifyInstallation() looks for a directory named for the component's
// version.
class DynamicPatchComponentInstallerPolicy : public ComponentInstallerPolicy {
 public:
  // A function with the signature of InstallProductComponent().
  using InstallCallback = base::RepeatingCallback<ProductComponentInstallResult(
      const base::FilePath& inner_crx_path,
      const base::FilePath& user_data_dir)>;

  // Installs patches for `user_data_dir`, which must be the browser's User Data
  // directory, via InstallProductComponent(). `modules_dir`, which must not be
  // empty, is the directory into which setup.exe installs them (i.e.,
  // `chrome::DIR_CHILD_MODULE_ROOT`).
  DynamicPatchComponentInstallerPolicy(base::FilePath user_data_dir,
                                       base::FilePath modules_dir);

  DynamicPatchComponentInstallerPolicy(
      const DynamicPatchComponentInstallerPolicy&) = delete;
  DynamicPatchComponentInstallerPolicy& operator=(
      const DynamicPatchComponentInstallerPolicy&) = delete;
  ~DynamicPatchComponentInstallerPolicy() override;

 private:
  friend class DynamicPatchComponentInstallerTest;

  // Like the public constructor, but installs patches via `install_callback`
  // rather than InstallProductComponent(). For use by tests.
  DynamicPatchComponentInstallerPolicy(base::FilePath user_data_dir,
                                       base::FilePath modules_dir,
                                       InstallCallback install_callback);

  // ComponentInstallerPolicy:
  bool VerifyInstallation(const base::DictValue& manifest,
                          const base::FilePath& install_dir) const override;
  bool SupportsGroupPolicyEnabledComponentUpdates() const override;
  bool RequiresNetworkEncryption() const override;
  update_client::CrxInstaller::Result OnCustomInstall(
      const base::DictValue& manifest,
      const base::FilePath& install_dir) override;
  void OnCustomUninstall() override;
  void ComponentReady(const base::Version& version,
                      const base::FilePath& install_dir,
                      base::DictValue manifest) override;
  base::FilePath GetRelativeInstallDir() const override;
  void GetHash(std::vector<uint8_t>* hash) const override;
  std::string GetName() const override;
  update_client::InstallerAttributes GetInstallerAttributes() const override;

  const base::FilePath user_data_dir_;
  const base::FilePath modules_dir_;
  const InstallCallback install_callback_;
};

// Registers the dynamic patch component with `cus` if dynamic patching is
// enabled, the running browser is the one that is installed, and
// `chrome::DIR_CHILD_MODULE_ROOT` is set. This is a no-op in non-Google Chrome
// builds.
void MaybeRegisterDynamicPatchComponent(ComponentUpdateService* cus);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_DYNAMIC_PATCH_COMPONENT_INSTALLER_H_
