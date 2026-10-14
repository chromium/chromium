// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/dynamic_patch_component_installer.h"

#include <stdint.h>

#include <array>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/timer/elapsed_timer.h"
#include "base/values.h"
#include "base/version.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/child_module/features.h"
#include "chrome/common/child_module/child_module_helper.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/util/helper.h"
#include "components/component_updater/component_installer.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"
#include "crypto/hash.h"

namespace component_updater {

namespace {

// The SHA256 of the SubjectPublicKeyInfo used to sign the component. The CRX ID
// is binbghhnflgglfabhjocbobkiignbgfh. setup.exe requires that the inner CRX be
// signed with the same key (see kDynamicPatchPublicKeySHA256 in
// chrome/installer/setup/install_component.cc).
constexpr std::array<uint8_t, crypto::hash::kSha256Size>
    kDynamicPatchPublicKeySHA256 = {
        0x18, 0xd1, 0x67, 0x7d, 0x5b, 0x66, 0xb5, 0x01, 0x79, 0xe2, 0x1e,
        0x1a, 0x88, 0x6d, 0x16, 0x57, 0x04, 0x67, 0x63, 0xf0, 0xa2, 0xd1,
        0x0e, 0x39, 0xbf, 0x6d, 0xeb, 0x45, 0x33, 0xe8, 0xda, 0x82};

// The name of the inner CRX within the component.
constexpr base::FilePath::CharType kInnerCrxFileName[] =
    FILE_PATH_LITERAL("chrome_dynamic_patch.crx3");

}  // namespace

DynamicPatchComponentInstallerPolicy::DynamicPatchComponentInstallerPolicy(
    base::FilePath user_data_dir,
    base::FilePath modules_dir)
    : DynamicPatchComponentInstallerPolicy(
          std::move(user_data_dir),
          std::move(modules_dir),
          base::BindRepeating(&InstallProductComponent)) {}

DynamicPatchComponentInstallerPolicy::DynamicPatchComponentInstallerPolicy(
    base::FilePath user_data_dir,
    base::FilePath modules_dir,
    InstallCallback install_callback)
    : user_data_dir_(std::move(user_data_dir)),
      modules_dir_(std::move(modules_dir)),
      install_callback_(std::move(install_callback)) {
  CHECK(!user_data_dir_.empty());
  CHECK(!modules_dir_.empty());
  CHECK(install_callback_);
}

DynamicPatchComponentInstallerPolicy::~DynamicPatchComponentInstallerPolicy() =
    default;

bool DynamicPatchComponentInstallerPolicy::VerifyInstallation(
    const base::DictValue& manifest,
    const base::FilePath& install_dir) const {
  const std::string* version_string = manifest.FindString("version");
  if (!version_string) {
    return false;
  }
  const base::Version patch_version(*version_string);
  if (!patch_version.IsValid()) {
    return false;
  }
  // Patches are installed within the directory of the browser version that they
  // patch, so verification fails once the browser has been updated to a new
  // version. The component installer then deletes this version of the
  // component and registers the component at version 0.0.0.0 so that a patch
  // for the new version can be installed. The CRX cache retains this version's
  // CRX, so that update can still be differential.
  const base::FilePath patch_dir =
      modules_dir_.AppendASCII(patch_version.GetString());
  // setup.exe copies the manifest into place last, so its presence indicates
  // that the patch has been installed completely.
  return base::PathExists(child_module::GetManifestPath(patch_dir)) &&
         base::PathExists(patch_dir.Append(chrome::kRendererDll));
}

bool DynamicPatchComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  // Patches are a security measure and are therefore installed unconditionally.
  return false;
}

bool DynamicPatchComponentInstallerPolicy::RequiresNetworkEncryption() const {
  // Don't reveal which version(s) of the renderer the client is downloading to
  // those observing network traffic so that an adversary-in-the-middle cannot
  // use this knowledge against the client's best interests.
  return true;
}

update_client::CrxInstaller::Result
DynamicPatchComponentInstallerPolicy::OnCustomInstall(
    const base::DictValue& manifest,
    const base::FilePath& install_dir) {
  const base::FilePath inner_crx = install_dir.Append(kInnerCrxFileName);
  const std::string_view install_level =
      install_static::IsSystemInstall() ? "SystemLevel" : "UserLevel";

  const base::ElapsedTimer timer;
  const ProductComponentInstallResult result =
      install_callback_.Run(inner_crx, user_data_dir_);
  base::UmaHistogramMediumTimes(
      base::StrCat(
          {"ComponentUpdater.DynamicPatch.InstallDuration.", install_level}),
      timer.Elapsed());
  base::UmaHistogramEnumeration(
      base::StrCat(
          {"ComponentUpdater.DynamicPatch.InstallationResult.", install_level}),
      result);

  // setup.exe operates on its own copy of the inner CRX. Delete this one so
  // that it doesn't occupy space in the User Data directory for as long as this
  // version of the component is installed.
  base::DeleteFile(inner_crx);

  // kAlreadyExists means that setup.exe found this or a newer version of the
  // patch already installed for this user and User Data directory (e.g., if a
  // previous installation completed but the browser exited before the
  // component updater recorded it). VerifyInstallation() determines whether
  // this version is present.
  if (result != ProductComponentInstallResult::kSuccess &&
      result != ProductComponentInstallResult::kAlreadyExists) {
    return update_client::CrxInstaller::Result(update_client::CategorizedError{
        .category = update_client::ErrorCategory::kInstaller,
        .code = static_cast<int>(result),
    });
  }
  return update_client::CrxInstaller::Result(0);
}

void DynamicPatchComponentInstallerPolicy::OnCustomUninstall() {}

void DynamicPatchComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    base::DictValue manifest) {
  // ChildModuleManager watches the directory into which setup.exe installs
  // patches, so there is nothing more to do here.
}

base::FilePath DynamicPatchComponentInstallerPolicy::GetRelativeInstallDir()
    const {
  return base::FilePath(FILE_PATH_LITERAL("ChildModules"));
}

void DynamicPatchComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign_range(kDynamicPatchPublicKeySHA256);
}

std::string DynamicPatchComponentInstallerPolicy::GetName() const {
  static constexpr char kDynamicPatchManifestName[] = "Chrome Dynamic Patch";

  return kDynamicPatchManifestName;
}

update_client::InstallerAttributes
DynamicPatchComponentInstallerPolicy::GetInstallerAttributes() const {
  return update_client::InstallerAttributes();
}

void MaybeRegisterDynamicPatchComponent(ComponentUpdateService* cus) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (!base::FeatureList::IsEnabled(child_module::features::kDynamicPatching)) {
    return;
  }
  // setup.exe installs patches into the registered installation of Chrome,
  // whereas the browser looks for them in its own directory. These are the
  // same only if the running browser is the registered one.
  if (!installer::IsCurrentProcessInstalled()) {
    return;
  }
  base::FilePath user_data_dir;
  if (!base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir)) {
    return;
  }
  // setup.exe derives the same user path component as the browser from the
  // canonical User Data directory that InstallProductComponent() provides, so
  // it installs patches where the browser looks for them. There is no point in
  // installing patches that the browser cannot find.
  base::FilePath modules_dir;
  if (!base::PathService::Get(chrome::DIR_CHILD_MODULE_ROOT, &modules_dir)) {
    return;
  }
  base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<DynamicPatchComponentInstallerPolicy>(
          std::move(user_data_dir), std::move(modules_dir)))
      ->Register(cus, base::OnceClosure());
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

}  // namespace component_updater
