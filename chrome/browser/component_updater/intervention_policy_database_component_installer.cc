// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/intervention_policy_database_component_installer.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/values.h"
#include "chrome/browser/resource_coordinator/intervention_policy_database.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/component_updater/component_updater_service.h"
#include "components/crx_file/id_util.h"

using component_updater::ComponentUpdateService;

namespace {

// The SHA256 of the SubjectPublicKeyInfo used to sign the component.
// The component id is: copjbmjbojbakpaedmpkhmiplmmehfck
const uint8_t kInterventionPolicyDatabasePublicKeySHA256[32] = {
    0x2e, 0xf9, 0x1c, 0x91, 0xe9, 0x10, 0xaf, 0x04, 0x3c, 0xfa, 0x7c,
    0x8f, 0xbc, 0xc4, 0x75, 0x2a, 0x48, 0x9a, 0x64, 0x74, 0xc6, 0xda,
    0xb7, 0xb9, 0xdf, 0x5f, 0x51, 0x3e, 0x50, 0x39, 0x04, 0xab};

// The name of the component, used in the chrome://components page.
const char kInterventionPolicyDatabaseComponentName[] =
    "Intervention Policy Database";

// The name of the database file inside of an installation of this component.
const base::FilePath::CharType kInterventionPolicyDatabaseBinaryPbFileName[] =
    FILE_PATH_LITERAL("intervention_policy_database.pb");

}  // namespace

namespace component_updater {

InterventionPolicyDatabaseComponentInstallerPolicy::
    InterventionPolicyDatabaseComponentInstallerPolicy(
        resource_coordinator::InterventionPolicyDatabase* database)
    : database_(database) {
  DCHECK(database_);
}

bool InterventionPolicyDatabaseComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return true;
}

bool InterventionPolicyDatabaseComponentInstallerPolicy::
    RequiresNetworkEncryption() const {
  // Public data is delivered via this component, no need for encryption.
  return false;
}

update_client::CrxInstaller::Result
InterventionPolicyDatabaseComponentInstallerPolicy::OnCustomInstall(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(0);
}

void InterventionPolicyDatabaseComponentInstallerPolicy::OnCustomUninstall() {}

// Called during startup and installation before ComponentReady().
bool InterventionPolicyDatabaseComponentInstallerPolicy::VerifyInstallation(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) const {
  return base::PathExists(
      install_dir.Append(kInterventionPolicyDatabaseBinaryPbFileName));
}

// NOTE: This is always called on the main UI thread. It is called once every
// startup to notify of an already installed component, and may be called
// repeatedly after that every time a new component is ready.
void InterventionPolicyDatabaseComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    base::Value::Dict manifest) {
  DCHECK(database_);
  database_->InitializeDatabaseWithProtoFile(
      install_dir.Append(kInterventionPolicyDatabaseBinaryPbFileName), version,
      std::move(manifest));
}

base::FilePath
InterventionPolicyDatabaseComponentInstallerPolicy::GetRelativeInstallDir()
    const {
  return base::FilePath(FILE_PATH_LITERAL("InterventionPolicyDatabase"));
}

void InterventionPolicyDatabaseComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign(kInterventionPolicyDatabasePublicKeySHA256,
               kInterventionPolicyDatabasePublicKeySHA256 +
                   std::size(kInterventionPolicyDatabasePublicKeySHA256));
}

std::string InterventionPolicyDatabaseComponentInstallerPolicy::GetName()
    const {
  return kInterventionPolicyDatabaseComponentName;
}

update_client::InstallerAttributes
InterventionPolicyDatabaseComponentInstallerPolicy::GetInstallerAttributes()
    const {
  return update_client::InstallerAttributes();
}

void RegisterInterventionPolicyDatabaseComponent(
    ComponentUpdateService* cus,
    resource_coordinator::InterventionPolicyDatabase* database) {
  std::unique_ptr<ComponentInstallerPolicy> policy(
      new InterventionPolicyDatabaseComponentInstallerPolicy(database));
  auto installer = base::MakeRefCounted<ComponentInstaller>(std::move(policy));
  installer->Register(cus, base::OnceClosure());
}

}  // namespace component_updater
