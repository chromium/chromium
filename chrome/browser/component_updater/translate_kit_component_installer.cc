// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/translate_kit_component_installer.h"

#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/services/on_device_translation/public/cpp/features.h"
#include "components/component_updater/component_updater_service.h"
#include "components/crx_file/id_util.h"
#include "components/update_client/update_client_errors.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/sha2.h"

namespace component_updater {

namespace {

// The SHA256 of the SubjectPublicKeyInfo used to sign the component.
// The component id is: lbimbicckdokpoicboneldipejkhjgdg
constexpr uint8_t kTranslateKitPublicKeySHA256[32] = {
    0x6c, 0x62, 0x69, 0x6d, 0x62, 0x69, 0x63, 0x63, 0x6b, 0x64, 0x6f,
    0x6b, 0x70, 0x6f, 0x69, 0x63, 0x62, 0x6f, 0x6e, 0x65, 0x6c, 0x64,
    0x69, 0x70, 0x65, 0x6a, 0x6b, 0x68, 0x6a, 0x67, 0x64, 0x67};

static_assert(std::size(kTranslateKitPublicKeySHA256) == crypto::kSHA256Length,
              "Wrong hash length");

// Location of the libtranslatekit binary within the installation directory.
#if BUILDFLAG(IS_WIN)
constexpr base::FilePath::CharType kTranslateKitBinaryRelativePath[] =
    FILE_PATH_LITERAL("TranslateKitFiles/libtranslatekit.dll");
#else
constexpr base::FilePath::CharType kTranslateKitBinaryRelativePath[] =
    FILE_PATH_LITERAL("TranslateKitFiles/libtranslatekit.so");
#endif

// Location of the TranslateKit component relative to the components directory.
constexpr base::FilePath::CharType
    kTranslateKitComponentInstallationRelativePath[] =
        FILE_PATH_LITERAL("translatekit");

// The manifest name of the TranslateKit component.
constexpr char kTranslateKitManifestName[] = "TranslateKit Library";

base::FilePath GetInstalledPath(const base::FilePath& base) {
  return base.Append(kTranslateKitBinaryRelativePath);
}

}  // namespace

TranslateKitComponentInstallerPolicy::TranslateKitComponentInstallerPolicy() =
    default;

TranslateKitComponentInstallerPolicy::~TranslateKitComponentInstallerPolicy() =
    default;

bool TranslateKitComponentInstallerPolicy::VerifyInstallation(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) const {
  return base::PathExists(GetInstalledPath(install_dir));
}

bool TranslateKitComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return true;
}

bool TranslateKitComponentInstallerPolicy::RequiresNetworkEncryption() const {
  return false;
}

update_client::CrxInstaller::Result
TranslateKitComponentInstallerPolicy::OnCustomInstall(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) {
  // Nothing custom here.
  return update_client::CrxInstaller::Result(0);
}

void TranslateKitComponentInstallerPolicy::OnCustomUninstall() {}

void TranslateKitComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    base::Value::Dict manifest) {
  VLOG(1) << "Component ready, version " << version.GetString() << " in "
          << install_dir.value();

  // TODO(crbug.com/362123222): Notify TranslationAPI controller.
}

// The base directory on Windows looks like:
// <profile>\AppData\Local\Google\Chrome\User Data\translatekit\.
base::FilePath TranslateKitComponentInstallerPolicy::GetRelativeInstallDir()
    const {
  return base::FilePath(kTranslateKitComponentInstallationRelativePath);
}

void TranslateKitComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign(
      kTranslateKitPublicKeySHA256,
      kTranslateKitPublicKeySHA256 + std::size(kTranslateKitPublicKeySHA256));
}

std::string TranslateKitComponentInstallerPolicy::GetName() const {
  return kTranslateKitManifestName;
}

update_client::InstallerAttributes
TranslateKitComponentInstallerPolicy::GetInstallerAttributes() const {
  return update_client::InstallerAttributes();
}

void RegisterTranslateKitComponent(ComponentUpdateService* cus) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!base::FeatureList::IsEnabled(
          on_device_translation::kEnableTranslateKitComponent)) {
    return;
  }

  VLOG(1) << "Registering TranslateKit component.";
  // TODO(crbug.com/362123222): Update when adding language model installer.
  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<TranslateKitComponentInstallerPolicy>());
  installer->Register(cus, base::OnceClosure());
}

}  // namespace component_updater
