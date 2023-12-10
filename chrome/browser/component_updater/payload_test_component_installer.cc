// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/payload_test_component_installer.h"

#include <cstdint>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "base/version.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/sha2.h"

namespace component_updater {

namespace {
constexpr char kManifestName[] = "Payload Test";
constexpr base::FilePath::CharType kInstallationRelativePath[] =
    FILE_PATH_LITERAL("PayloadTest");
constexpr base::FilePath::CharType kPayloadRelativePath[] =
    FILE_PATH_LITERAL("payload.bin");
constexpr uint8_t kPublicKeySHA256[32] = {
    0xac, 0xf2, 0xd5, 0x6a, 0x3c, 0x14, 0xcf, 0xff, 0x51, 0x00, 0x97,
    0xb9, 0x8c, 0x57, 0x4a, 0x30, 0xe6, 0x25, 0x91, 0xca, 0xbe, 0x8d,
    0x01, 0xb9, 0x60, 0x0d, 0x8c, 0x86, 0x83, 0xb5, 0x8a, 0xfe};
static_assert(std::size(kPublicKeySHA256) == crypto::kSHA256Length);
}  // namespace

bool PayloadTestComponentInstallerPolicy::VerifyInstallation(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) const {
  return base::PathExists(install_dir.Append(kPayloadRelativePath));
}

bool PayloadTestComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return true;
}

bool PayloadTestComponentInstallerPolicy::RequiresNetworkEncryption() const {
  return true;
}

update_client::CrxInstaller::Result
PayloadTestComponentInstallerPolicy::OnCustomInstall(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(update_client::InstallError::NONE);
}

void PayloadTestComponentInstallerPolicy::OnCustomUninstall() {}

void PayloadTestComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    base::Value::Dict manifest) {
  VLOG(1) << "Component ready, version " << version.GetString() << " in "
          << install_dir.value();
}

base::FilePath PayloadTestComponentInstallerPolicy::GetRelativeInstallDir()
    const {
  return base::FilePath(kInstallationRelativePath);
}

void PayloadTestComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign(std::begin(kPublicKeySHA256), std::end(kPublicKeySHA256));
}

std::string PayloadTestComponentInstallerPolicy::GetName() const {
  return kManifestName;
}

update_client::InstallerAttributes
PayloadTestComponentInstallerPolicy::GetInstallerAttributes() const {
  return update_client::InstallerAttributes();
}

bool PayloadTestComponentInstallerPolicy::AllowCachedCopies() const {
  return false;
}

void RegisterPayloadTestComponent(ComponentUpdateService* cus) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<PayloadTestComponentInstallerPolicy>())
      ->Register(cus, base::DoNothing());
}

}  // namespace component_updater
