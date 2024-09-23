// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/component_updater/screen_ai_component_installer.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/screen_ai/screen_ai_install_state.h"
#include "components/component_updater/component_updater_service.h"
#include "components/crx_file/id_util.h"
#include "components/update_client/update_client_errors.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/sha2.h"
#include "services/screen_ai/public/cpp/utilities.h"

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
  VLOG(1) << "Screen AI Component ready, version " << version.GetString()
          << " in " << install_dir.value();

  screen_ai::ScreenAIInstallState::GetInstance()->SetComponentFolder(
      install_dir);
}

bool ScreenAIComponentInstallerPolicy::VerifyInstallation(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) const {
  VLOG(1) << "Verifying Screen AI component in " << install_dir.value();

  base::Version version;
  DCHECK(!version.IsValid());

  const base::Value* version_value = manifest.Find("version");
  if (version_value && version_value->is_string()) {
    version = base::Version(version_value->GetString());
  }

  return screen_ai::ScreenAIInstallState::VerifyLibraryVersion(version);
}

base::FilePath ScreenAIComponentInstallerPolicy::GetRelativeInstallDir() const {
  return screen_ai::GetRelativeInstallDir();
}

// static
std::string ScreenAIComponentInstallerPolicy::GetOmahaId() {
  return crx_file::id_util::GenerateIdFromHash(kScreenAIPublicKeySHA256);
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
  base::DeletePathRecursively(screen_ai::GetComponentDir());
}

void ManageScreenAIComponentRegistration(ComponentUpdateService* cus,
                                         PrefService* local_state) {
  if (screen_ai::ScreenAIInstallState::ShouldInstall(local_state)) {
    RegisterScreenAIComponent(cus);
    return;
  }

  // Clean up.
  if (base::PathExists(screen_ai::GetComponentDir())) {
    ScreenAIComponentInstallerPolicy::DeleteComponent();
  }
}

void RegisterScreenAIComponent(ComponentUpdateService* cus) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Only register once.
  if (screen_ai::ScreenAIInstallState::GetInstance()->get_state() !=
      screen_ai::ScreenAIInstallState::State::kNotDownloaded) {
    return;
  }
  screen_ai::ScreenAIInstallState::GetInstance()->SetState(
      screen_ai::ScreenAIInstallState::State::kDownloading);

  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<ScreenAIComponentInstallerPolicy>());
  installer->Register(cus, base::OnceClosure());
}

}  // namespace component_updater
