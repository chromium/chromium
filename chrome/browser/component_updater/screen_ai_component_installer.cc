// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/screen_ai_component_installer.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/screen_ai/screen_ai_install_state.h"
#include "components/component_updater/component_updater_service.h"
#include "components/crx_file/id_util.h"
#include "components/services/screen_ai/public/cpp/utilities.h"
#include "components/update_client/update_client_errors.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/sha2.h"

using content::BrowserThread;

namespace {

// The SHA256 of the SubjectPublicKeyInfo used to sign the component.
// The component id is: mfhmdacoffpmifoibamicehhklffanao
constexpr uint8_t kScreenAIPublicKeySHA256[32] = {
    0xc5, 0x7c, 0x30, 0x2e, 0x55, 0xfc, 0x85, 0xe8, 0x10, 0xc8, 0x24,
    0x77, 0xab, 0x55, 0x0d, 0x0e, 0x5a, 0xed, 0x04, 0x7b, 0x1e, 0x16,
    0x86, 0x7c, 0xf0, 0x42, 0x71, 0x85, 0xe4, 0x31, 0x2d, 0xc5};

static_assert(std::size(kScreenAIPublicKeySHA256) == crypto::kSHA256Length,
              "Wrong hash length");

constexpr char kScreenAIManifestName[] = "ScreenAI Library";

}  // namespace

namespace component_updater {

ScreenAIComponentInstallerPolicy::ScreenAIComponentInstallerPolicy() = default;

ScreenAIComponentInstallerPolicy::~ScreenAIComponentInstallerPolicy() = default;

bool ScreenAIComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return true;
}

bool ScreenAIComponentInstallerPolicy::RequiresNetworkEncryption() const {
  // This component is deployed only to the users of assistive technologies.
  // Tracking it reveals personal information about the user.
  return true;
}

update_client::CrxInstaller::Result
ScreenAIComponentInstallerPolicy::OnCustomInstall(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(update_client::InstallError::NONE);
}

void ScreenAIComponentInstallerPolicy::OnCustomUninstall() {}

void ScreenAIComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    base::Value::Dict manifest) {
  screen_ai::ScreenAIInstallState::GetInstance()->SetComponentFolder(
      install_dir);
  VLOG(1) << "Screen AI Component ready, version " << version.GetString()
          << " in " << install_dir.value();
}

bool ScreenAIComponentInstallerPolicy::VerifyInstallation(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) const {
  VLOG(1) << "Verifying Screen AI component in " << install_dir.value();

  const base::Value* version_value = manifest.Find("version");
  if (!version_value || !version_value->is_string()) {
    VLOG(0) << "Cannot verify Screen AI library version.";
    return false;
  }
  if (!screen_ai::ScreenAIInstallState::VerifyLibraryVersion(
          version_value->GetString())) {
    return false;
  }

  // Check the file iterator heuristic to find the library in the sandbox
  // returns the same directory as `install_dir`.
  return screen_ai::GetLatestComponentBinaryPath().DirName() == install_dir;
}

base::FilePath ScreenAIComponentInstallerPolicy::GetRelativeInstallDir() const {
  return screen_ai::GetRelativeInstallDir();
}

void ScreenAIComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign(kScreenAIPublicKeySHA256,
               kScreenAIPublicKeySHA256 + std::size(kScreenAIPublicKeySHA256));
}

std::string ScreenAIComponentInstallerPolicy::GetName() const {
  return kScreenAIManifestName;
}

update_client::InstallerAttributes
ScreenAIComponentInstallerPolicy::GetInstallerAttributes() const {
  return update_client::InstallerAttributes();
}

// static
void ScreenAIComponentInstallerPolicy::DeleteComponent() {
  base::FilePath component_binary_path =
      screen_ai::GetLatestComponentBinaryPath();

  if (!component_binary_path.empty())
    base::DeletePathRecursively(component_binary_path.DirName());
}

void RegisterScreenAIComponent(ComponentUpdateService* cus,
                               PrefService* local_state) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (screen_ai::ScreenAIInstallState::ShouldInstall(local_state)) {
    screen_ai::ScreenAIInstallState::GetInstance()->SetState(
        screen_ai::ScreenAIInstallState::State::kDownloading);

    auto installer = base::MakeRefCounted<ComponentInstaller>(
        std::make_unique<ScreenAIComponentInstallerPolicy>());
    installer->Register(cus, base::OnceClosure());
    return;
  }

  // Clean up.
  if (!screen_ai::GetLatestComponentBinaryPath().empty()) {
    ScreenAIComponentInstallerPolicy::DeleteComponent();
  }
}

}  // namespace component_updater
