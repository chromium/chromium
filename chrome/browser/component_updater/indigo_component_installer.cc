// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/indigo_component_installer.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_list.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/version.h"
#include "chrome/common/chrome_features.h"
#include "components/component_updater/component_updater_service.h"
#include "content/public/browser/browser_thread.h"

namespace {

// The SHA256 of the SubjectPublicKeyInfo used to sign the extension.
constexpr uint8_t kIndigoPublicKeySHA256[32] = {
    0xa6, 0x49, 0xd8, 0x85, 0x91, 0x1f, 0xee, 0x25, 0x41, 0xb2, 0x49,
    0x5c, 0xc9, 0x55, 0x15, 0x31, 0xf9, 0x27, 0x2a, 0x85, 0x37, 0xd0,
    0xae, 0xc8, 0x57, 0x4d, 0x81, 0x91, 0x2d, 0xe7, 0x31, 0x9b};

constexpr char kIndigoManifestName[] = "Indigo";

constexpr base::FilePath::CharType kContentScriptFileName[] =
    FILE_PATH_LITERAL("content_script.js");

base::FilePath& GetInstallDirStorage() {
  static base::NoDestructor<base::FilePath> install_dir;
  return *install_dir;
}

}  // namespace

namespace component_updater {

namespace {
base::RepeatingCallbackList<void()>& GetCallbackList() {
  static base::NoDestructor<base::RepeatingCallbackList<void()>> callbacks;
  return *callbacks;
}
}  // namespace

base::CallbackListSubscription RegisterIndigoComponentReadyCallback(
    base::RepeatingClosure callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return GetCallbackList().Add(std::move(callback));
}

IndigoComponentInstallerPolicy::IndigoComponentInstallerPolicy() = default;

IndigoComponentInstallerPolicy::~IndigoComponentInstallerPolicy() = default;

bool IndigoComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return true;
}

bool IndigoComponentInstallerPolicy::RequiresNetworkEncryption() const {
  return false;
}

update_client::CrxInstaller::Result
IndigoComponentInstallerPolicy::OnCustomInstall(
    const base::DictValue& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(update_client::InstallError::NONE);
}

void IndigoComponentInstallerPolicy::OnCustomUninstall() {}

void IndigoComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    base::DictValue manifest) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  VLOG(1) << "Component ready, version " << version.GetString() << " in "
          << install_dir;
  GetInstallDirStorage() = install_dir;

  GetCallbackList().Notify();
}

// Called during startup and installation before ComponentReady().
bool IndigoComponentInstallerPolicy::VerifyInstallation(
    const base::DictValue& manifest,
    const base::FilePath& install_dir) const {
  return base::PathExists(install_dir.Append(kContentScriptFileName));
}

base::FilePath IndigoComponentInstallerPolicy::GetRelativeInstallDir() const {
  return base::FilePath(FILE_PATH_LITERAL("indigo"));
}

void IndigoComponentInstallerPolicy::GetHash(std::vector<uint8_t>* hash) const {
  hash->assign(std::begin(kIndigoPublicKeySHA256),
               std::end(kIndigoPublicKeySHA256));
}

std::string IndigoComponentInstallerPolicy::GetName() const {
  return kIndigoManifestName;
}

update_client::InstallerAttributes
IndigoComponentInstallerPolicy::GetInstallerAttributes() const {
  update_client::InstallerAttributes attributes;
  std::string attribute = features::kIndigoComponentAttribute.Get();
  if (!attribute.empty()) {
    attributes["indigo"] = attribute;
  }
  return attributes;
}

void RegisterIndigoComponent(ComponentUpdateService* cus) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!base::FeatureList::IsEnabled(features::kIndigoComponent)) {
    return;
  }
  VLOG(1) << "Registering Indigo component.";
  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<IndigoComponentInstallerPolicy>());
  installer->Register(cus, base::DoNothing());
}

std::optional<base::FilePath> GetIndigoComponentInstallDir() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const base::FilePath& install_dir = GetInstallDirStorage();
  if (install_dir.empty()) {
    return std::nullopt;
  }
  return install_dir;
}

std::optional<base::FilePath> GetIndigoContentScriptPath() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::optional<base::FilePath> install_dir = GetIndigoComponentInstallDir();
  if (!install_dir.has_value()) {
    return std::nullopt;
  }
  return install_dir->Append(kContentScriptFileName);
}

void ResetIndigoInstallDirForTesting() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  GetInstallDirStorage() = base::FilePath();
}

}  // namespace component_updater
