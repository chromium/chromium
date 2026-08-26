// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/dictation_connector_component_installer.h"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/dictation/features.h"
#include "chrome/common/extensions/extension_constants.h"
#include "components/component_updater/component_updater_service.h"
#include "components/crx_file/id_util.h"
#include "content/public/browser/browser_thread.h"

namespace {

// The SHA256 of the SubjectPublicKeyInfo used to sign the component.
// The component's id is: kbglekiebdohdafflpmiejhbfdmjdbbe
const uint8_t kDictationConnectorPublicKeySHA256[32] = {
    0xa1, 0x6b, 0x4a, 0x84, 0x13, 0xe7, 0x30, 0x55, 0xbf, 0xc8, 0x49,
    0x71, 0x53, 0xc9, 0x31, 0x14, 0x7c, 0xaa, 0x56, 0x41, 0xd3, 0xa6,
    0x25, 0xb9, 0x75, 0xec, 0xd4, 0xcb, 0xe1, 0xb9, 0xbf, 0x81};

const char kDictationConnectorManifestName[] = "Dictation Connector";

// The relative path where the component will be installed.
const base::FilePath::CharType kDictationConnectorRelativeInstallDir[] =
    FILE_PATH_LITERAL("DictationConnector");

// Files in the extension
const base::FilePath::CharType kBackgroundCompiledFileName[] =
    FILE_PATH_LITERAL("background_compiled.js");
const base::FilePath::CharType kManifestFileName[] =
    FILE_PATH_LITERAL("manifest.json");
const base::FilePath::CharType kOffscreenCompiledFileName[] =
    FILE_PATH_LITERAL("offscreen_compiled.js");
const base::FilePath::CharType kOffscreenHtmlFileName[] =
    FILE_PATH_LITERAL("offscreen.html");

class DictationConnectorDirectory {
 public:
  static DictationConnectorDirectory& GetInstance() {
    static base::NoDestructor<DictationConnectorDirectory> directory;
    return *directory.get();
  }

  base::CallbackListSubscription Get(
      base::OnceCallback<void(const base::FilePath&)> callback) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    // Use BindPostTaskToCurrentDefault to ensure the callback is posted
    // asynchronously when run from Notify.
    base::CallbackListSubscription subscription =
        callbacks_.Add(base::BindPostTaskToCurrentDefault(std::move(callback)));
    if (!dir_.empty()) {
      NotifyCallbacksAsync();
    }
    return subscription;
  }

  void Set(const base::FilePath& new_dir) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    CHECK(!new_dir.empty());
    dir_ = new_dir;
    NotifyCallbacksAsync();
  }

  void ResetForTesting() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    dir_ = base::FilePath();
  }

 private:
  void NotifyCallbacksAsync() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (!dir_.empty()) {
      callbacks_.Notify(dir_);
    }
  }

  base::FilePath dir_;
  base::OnceCallbackList<void(const base::FilePath&)> callbacks_;
};

}  // namespace

namespace component_updater {

DictationConnectorComponentInstallerPolicy::
    DictationConnectorComponentInstallerPolicy() = default;

bool DictationConnectorComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return true;
}

bool DictationConnectorComponentInstallerPolicy::RequiresNetworkEncryption()
    const {
  return false;
}

update_client::CrxInstaller::Result
DictationConnectorComponentInstallerPolicy::OnCustomInstall(
    const base::DictValue& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(update_client::InstallError::NONE);
}

void DictationConnectorComponentInstallerPolicy::OnCustomUninstall() {}

bool DictationConnectorComponentInstallerPolicy::VerifyInstallation(
    const base::DictValue& /* manifest */,
    const base::FilePath& install_dir) const {
  return base::PathExists(install_dir.Append(kBackgroundCompiledFileName)) &&
         base::PathExists(install_dir.Append(kManifestFileName)) &&
         base::PathExists(install_dir.Append(kOffscreenCompiledFileName)) &&
         base::PathExists(install_dir.Append(kOffscreenHtmlFileName));
}

void DictationConnectorComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    base::DictValue /* manifest */) {
  VLOG(1) << "Dictation Connector component ready, version "
          << version.GetString() << " in " << install_dir.value();

  DictationConnectorDirectory::GetInstance().Set(install_dir);
}

// static
base::CallbackListSubscription
DictationConnectorComponentInstallerPolicy::GetExtensionDirectory(
    base::OnceCallback<void(const base::FilePath&)> callback) {
  DCHECK(base::FeatureList::IsEnabled(dictation::kDictation));
  return DictationConnectorDirectory::GetInstance().Get(std::move(callback));
}

// static
void DictationConnectorComponentInstallerPolicy::ResetForTesting() {
  DictationConnectorDirectory::GetInstance().ResetForTesting();  // IN-TEST
}

base::FilePath
DictationConnectorComponentInstallerPolicy::GetRelativeInstallDir() const {
  return base::FilePath(kDictationConnectorRelativeInstallDir);
}

void DictationConnectorComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign_range(kDictationConnectorPublicKeySHA256);
}

std::string DictationConnectorComponentInstallerPolicy::GetName() const {
  return kDictationConnectorManifestName;
}

update_client::InstallerAttributes
DictationConnectorComponentInstallerPolicy::GetInstallerAttributes() const {
  return update_client::InstallerAttributes();
}

void RegisterDictationConnectorComponent(ComponentUpdateService* cus) {
  if (!base::FeatureList::IsEnabled(dictation::kDictation)) {
    return;
  }

  VLOG(1) << "Registering Dictation Connector component.";
  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<DictationConnectorComponentInstallerPolicy>());
  installer->Register(cus, base::OnceClosure());
}

}  // namespace component_updater
