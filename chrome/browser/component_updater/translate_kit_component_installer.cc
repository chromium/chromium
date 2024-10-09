// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/translate_kit_component_installer.h"

#include <memory>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/on_device_translation/constants.h"
#include "chrome/browser/on_device_translation/pref_names.h"
#include "components/component_updater/component_updater_service.h"
#include "components/crx_file/id_util.h"
#include "components/update_client/update_client_errors.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/sha2.h"

#if !BUILDFLAG(IS_ANDROID)
#include "components/services/on_device_translation/public/cpp/features.h"
#endif  // !BUILDFLAG(IS_ANDROID)

namespace component_updater {

namespace {

// The SHA256 of the SubjectPublicKeyInfo used to sign the component.
// The component id is: lbimbicckdokpoicboneldipejkhjgdg
constexpr uint8_t kTranslateKitPublicKeySHA256[32] = {
    0xb1, 0x8c, 0x18, 0x22, 0xa3, 0xea, 0xfe, 0x82, 0x1e, 0xd4, 0xb3,
    0x8f, 0x49, 0xa7, 0x96, 0x36, 0x55, 0xf3, 0xbc, 0x0d, 0xa5, 0x67,
    0x48, 0x09, 0xcd, 0x7b, 0xa9, 0x5f, 0xd8, 0x7f, 0x53, 0xb4};

static_assert(std::size(kTranslateKitPublicKeySHA256) == crypto::kSHA256Length,
              "Wrong hash length");

// The location of the libtranslatekit binary within the installation directory.
#if BUILDFLAG(IS_WIN)
constexpr base::FilePath::CharType kTranslateKitBinaryRelativePath[] =
    FILE_PATH_LITERAL("TranslateKitFiles/libtranslatekit.dll");
#else
constexpr base::FilePath::CharType kTranslateKitBinaryRelativePath[] =
    FILE_PATH_LITERAL("TranslateKitFiles/libtranslatekit.so");
#endif

// The manifest name of the TranslateKit component.
// This matches:
// - the manifest name in Automation.java from
//   go/newchromecomponent#server-side-setup.
// - the display name at http://omaharelease/2134318/settings.
constexpr char kTranslateKitManifestName[] = "Chrome TranslateKit";

// Returns the full path where the libtranslatekit binary will be installed.
//
// The installation path is under
//    <UserDataDir>/TranslateKit/lib/<version>/TranslateKitFiles/libtranslatekit.xx
// where <User Data Dir> can be determined by following the guide:
// https://chromium.googlesource.com/chromium/src.git/+/HEAD/docs/user_data_dir.md#current-location
base::FilePath GetInstalledPath(const base::FilePath& base) {
  return base.Append(kTranslateKitBinaryRelativePath);
}

}  // namespace

TranslateKitComponentInstallerPolicy::TranslateKitComponentInstallerPolicy(
    PrefService* pref_service)
    : pref_service_(pref_service) {}

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

#if !BUILDFLAG(IS_ANDROID)
  CHECK(pref_service_);
  pref_service_->SetFilePath(prefs::kTranslateKitBinaryPath,
                             GetInstalledPath(install_dir));
#endif  // !BUILDFLAG(IS_ANDROID)
}

base::FilePath TranslateKitComponentInstallerPolicy::GetRelativeInstallDir()
    const {
  return base::FilePath(
      on_device_translation::kTranslateKitBinaryInstallationRelativeDir);
}

void TranslateKitComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign(std::begin(kTranslateKitPublicKeySHA256),
               std::end(kTranslateKitPublicKeySHA256));
}

std::string TranslateKitComponentInstallerPolicy::GetName() const {
  return kTranslateKitManifestName;
}

update_client::InstallerAttributes
TranslateKitComponentInstallerPolicy::GetInstallerAttributes() const {
  return update_client::InstallerAttributes();
}

void RegisterTranslateKitComponent(ComponentUpdateService* cus,
                                   PrefService* pref_service) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  VLOG(1) << "Registering TranslateKit component.";
  // TODO(crbug.com/362123222): Update when adding language model installer.
  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<TranslateKitComponentInstallerPolicy>(pref_service));
  installer->Register(cus, base::OnceClosure());
}

}  // namespace component_updater
