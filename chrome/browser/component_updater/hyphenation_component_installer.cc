// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/hyphenation_component_installer.h"

#include "base/files/file_util.h"
#include "base/logging.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

using component_updater::ComponentUpdateService;

namespace {

// The SHA256 of the SubjectPublicKeyInfo used to sign the extension.
// The extension id is: jamhcnnkihinmdlkakkaopbjbbcngflc
constexpr uint8_t kHyphenationPublicKeySHA256[32] = {
    0x90, 0xc7, 0x2d, 0xda, 0x87, 0x8d, 0xc3, 0xba, 0x0a, 0xa0, 0xef,
    0x19, 0x11, 0x2d, 0x65, 0xb2, 0x04, 0xfc, 0x2e, 0x3e, 0xbb, 0x35,
    0x85, 0xae, 0x64, 0xca, 0x07, 0x61, 0x15, 0x12, 0x9e, 0xbc};

constexpr char kHyphenationManifestName[] = "Hyphenation";

constexpr base::FilePath::CharType kHyphenationRelativeInstallDir[] =
    FILE_PATH_LITERAL("hyphen-data");

}  // namespace

namespace component_updater {

HyphenationComponentInstallerPolicy::HyphenationComponentInstallerPolicy() =
    default;

HyphenationComponentInstallerPolicy::~HyphenationComponentInstallerPolicy() =
    default;

bool HyphenationComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  // False since this is a data, non-binary component.
  return false;
}

bool HyphenationComponentInstallerPolicy::RequiresNetworkEncryption() const {
  // Update checks and pings associated with this component do not require
  // confidentiality, since the component is identical for all users.
  return false;
}

update_client::CrxInstaller::Result
HyphenationComponentInstallerPolicy::OnCustomInstall(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(0);  // Nothing custom here.
}

void HyphenationComponentInstallerPolicy::OnCustomUninstall() {}

void HyphenationComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    std::unique_ptr<base::DictionaryValue> manifest) {
  VLOG(1) << "Hyphenation Component ready, version " << version.GetString()
          << " in " << install_dir.value();
  // TODO(kojii): Pass `install_dir` to `hyphenation_impl.cc`.
}

// Called during startup and installation before ComponentReady().
bool HyphenationComponentInstallerPolicy::VerifyInstallation(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) const {
  return true;
}

base::FilePath HyphenationComponentInstallerPolicy::GetRelativeInstallDir()
    const {
  return base::FilePath(kHyphenationRelativeInstallDir);
}

void HyphenationComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign(
      kHyphenationPublicKeySHA256,
      kHyphenationPublicKeySHA256 + base::size(kHyphenationPublicKeySHA256));
}

std::string HyphenationComponentInstallerPolicy::GetName() const {
  return kHyphenationManifestName;
}

update_client::InstallerAttributes
HyphenationComponentInstallerPolicy::GetInstallerAttributes() const {
  return update_client::InstallerAttributes();
}

std::vector<std::string> HyphenationComponentInstallerPolicy::GetMimeTypes()
    const {
  return {};
}

void RegisterHyphenationComponent(ComponentUpdateService* cus) {
  VLOG(1) << "Registering Hyphenation component.";
  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<HyphenationComponentInstallerPolicy>());
  installer->Register(cus, base::OnceClosure());
}

}  // namespace component_updater
