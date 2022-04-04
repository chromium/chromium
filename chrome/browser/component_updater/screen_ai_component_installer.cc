// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/screen_ai_component_installer.h"

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "components/component_updater/component_updater_service.h"
#include "components/crx_file/id_util.h"
#include "components/services/screen_ai/public/cpp/pref_names.h"
#include "components/services/screen_ai/public/cpp/utilities.h"
#include "components/update_client/update_client_errors.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/sha2.h"
#include "ui/accessibility/accessibility_features.h"

using content::BrowserThread;

namespace {

const int kScreenAICleanUpDelayInDays = 30;

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
    const base::Value& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(update_client::InstallError::NONE);
}

void ScreenAIComponentInstallerPolicy::OnCustomUninstall() {}

void ScreenAIComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    base::Value manifest) {
  VLOG(1) << "Screen AI Component ready, version " << version.GetString()
          << " in " << install_dir.value();
}

bool ScreenAIComponentInstallerPolicy::VerifyInstallation(
    const base::Value& manifest,
    const base::FilePath& install_dir) const {
  // TODO(https://crbug.com/1278249): Consider trying to open and initialize the
  // library.
  VLOG(1) << "Verifying Screen AI Library in " << install_dir.value();
  return screen_ai::GetLibraryFilePath().DirName() == install_dir;
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
void ScreenAIComponentInstallerPolicy::DeleteLibraryOrScheduleDeletionIfNeeded(
    PrefService* global_prefs) {
  base::FilePath library_path = screen_ai::GetLibraryFilePath();
  if (library_path.empty())
    return;

  base::Time deletion_time =
      global_prefs->GetTime(prefs::kScreenAIScheduledDeletionTimePrefName);

  // Set deletion time if it is not set yet.
  if (deletion_time.is_null()) {
    global_prefs->SetTime(
        prefs::kScreenAIScheduledDeletionTimePrefName,
        base::Time::Now() + base::Days(kScreenAICleanUpDelayInDays));
    return;
  }

  if (deletion_time <= base::Time::Now()) {
    // If there are more than one instance of the library, delete them as well.
    do {
      base::DeletePathRecursively(library_path.DirName());
      library_path = screen_ai::GetLibraryFilePath();
    } while (!library_path.empty());
    global_prefs->SetTime(prefs::kScreenAIScheduledDeletionTimePrefName,
                          base::Time());
  }
}

void RegisterScreenAIComponent(ComponentUpdateService* cus,
                               PrefService* global_prefs) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!features::IsScreenAIEnabled()) {
    ScreenAIComponentInstallerPolicy::DeleteLibraryOrScheduleDeletionIfNeeded(
        global_prefs);
    return;
  }

  // Remove scheduled time for deletion as feature is enabled.
  global_prefs->SetTime(prefs::kScreenAIScheduledDeletionTimePrefName,
                        base::Time());

  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<ScreenAIComponentInstallerPolicy>());
  installer->Register(cus, base::OnceClosure());
}

}  // namespace component_updater
