// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/pnacl_component_installer.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/atomicops.h"
#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_paths.h"
#include "components/component_updater/component_installer.h"
#include "components/component_updater/component_updater_service.h"
#include "components/component_updater/component_updater_utils.h"
#include "components/nacl/common/nacl_switches.h"
#include "components/update_client/update_query_params.h"
#include "components/update_client/utils.h"
#include "content/public/browser/browser_thread.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#endif

using content::BrowserThread;
using update_client::CrxComponent;
using update_client::UpdateQueryParams;

namespace component_updater {

namespace {

// Name of the Pnacl component specified in the manifest.
const char kPnaclManifestName[] = "PNaCl Translator";

constexpr uint8_t kPnaclPublicKeySHA256[32] = {
    // This corresponds to AppID: hnimpnehoodheedghdeeijklkeaacbdc
    0x7d, 0x8c, 0xfd, 0x47, 0xee, 0x37, 0x44, 0x36, 0x73, 0x44, 0x89,
    0xab, 0xa4, 0x00, 0x21, 0x32, 0x4a, 0x06, 0x06, 0xf1, 0x51, 0x3c,
    0x51, 0xba, 0x31, 0x2f, 0xbc, 0xb3, 0x99, 0x07, 0xdc, 0x9c};

// Sanitize characters from Pnacl Arch value so that they can be used
// in path names.  This should only be characters in the set: [a-z0-9_].
// Keep in sync with chrome/browser/nacl_host/nacl_file_host.
std::string SanitizeForPath(const std::string& input) {
  std::string result;
  base::ReplaceChars(input, "-", "_", &result);
  return result;
}

// If we don't have Pnacl installed, this is the version we claim.
const char kMinPnaclVersion[] = "0.46.0.4";

// Initially say that we do need OnDemand updates. If there is a version of
// PNaCl on disk, this will be updated by CheckVersionCompatiblity().
volatile base::subtle::Atomic32 needs_on_demand_update = 1;

void CheckVersionCompatiblity(const base::Version& current_version) {
  // Using NoBarrier, since needs_on_demand_update is standalone and does
  // not have other associated data.
  base::subtle::NoBarrier_Store(
      &needs_on_demand_update,
      current_version < base::Version(kMinPnaclVersion));
}

// PNaCl is packaged as a multi-CRX.  This returns the platform-specific
// subdirectory that is part of that multi-CRX.
base::FilePath GetPlatformDir(const base::FilePath& base_path) {
  std::string arch = SanitizeForPath(UpdateQueryParams::GetNaclArch());
  return base_path.AppendASCII("_platform_specific").AppendASCII(arch);
}

// Tell the rest of the world where to find the platform-specific PNaCl files.
void OverrideDirPnaclComponent(const base::FilePath& base_path) {
  base::PathService::Override(chrome::DIR_PNACL_COMPONENT,
                              GetPlatformDir(base_path));
}

std::optional<base::Value::Dict> ReadJSONManifest(
    const base::FilePath& manifest_path) {
  JSONFileValueDeserializer deserializer(manifest_path);
  std::string error;
  std::unique_ptr<base::Value> root = deserializer.Deserialize(nullptr, &error);
  if (!root.get()) {
    return std::nullopt;
  }
  if (!root->is_dict()) {
    return std::nullopt;
  }
  return std::move(*root).TakeDict();
}

// Read the PNaCl specific manifest.
std::optional<base::Value::Dict> ReadPnaclManifest(
    const base::FilePath& unpack_path) {
  base::FilePath manifest_path =
      GetPlatformDir(unpack_path).AppendASCII("pnacl_public_pnacl_json");
  if (!base::PathExists(manifest_path)) {
    return std::nullopt;
  }
  return ReadJSONManifest(manifest_path);
}

// Check that the component's manifest is for PNaCl, and check the
// PNaCl manifest indicates this is the correct arch-specific package.
bool CheckPnaclComponentManifest(const base::Value::Dict& manifest,
                                 const base::Value::Dict& pnacl_manifest) {
  // Make sure we have the right |manifest| file.
  const std::string* name = manifest.FindString("name");
  if (!name || !base::IsStringASCII(*name)) {
    LOG(WARNING) << "'name' field is missing from manifest!";
    return false;
  }
  // For the webstore, we've given different names to each of the
  // architecture specific packages (and test/QA vs not test/QA)
  // so only part of it is the same.
  if (name->find(kPnaclManifestName) == std::string::npos) {
    LOG(WARNING) << "'name' field in manifest is invalid (" << *name
                 << ") -- missing (" << kPnaclManifestName << ")";
    return false;
  }

  const std::string* proposed_version = manifest.FindString("version");
  if (!proposed_version || !base::IsStringASCII(*proposed_version)) {
    LOG(WARNING) << "'version' field is missing from manifest!";
    return false;
  }
  base::Version version(*proposed_version);
  if (!version.IsValid()) {
    LOG(WARNING) << "'version' field in manifest is invalid "
                 << version.GetString();
    return false;
  }

  // Now check the `pnacl_manifest`.
  const std::string* arch = pnacl_manifest.FindString("pnacl-arch");
  if (!arch || !base::IsStringASCII(*arch)) {
    LOG(WARNING) << "'pnacl-arch' field is missing from pnacl-manifest!";
    return false;
  }
  if (arch->compare(UpdateQueryParams::GetNaclArch()) != 0) {
    LOG(WARNING) << "'pnacl-arch' field in manifest is invalid (" << *arch
                 << " vs " << UpdateQueryParams::GetNaclArch() << ")";
    return false;
  }

  return true;
}

class PnaclComponentInstallerPolicy : public ComponentInstallerPolicy {
 public:
  PnaclComponentInstallerPolicy();

  PnaclComponentInstallerPolicy(const PnaclComponentInstallerPolicy&) = delete;
  PnaclComponentInstallerPolicy& operator=(
      const PnaclComponentInstallerPolicy&) = delete;

  ~PnaclComponentInstallerPolicy() override;

 private:
  // ComponentInstallerPolicy implementation.
  bool SupportsGroupPolicyEnabledComponentUpdates() const override;
  bool RequiresNetworkEncryption() const override;
  update_client::CrxInstaller::Result OnCustomInstall(
      const base::Value::Dict& manifest,
      const base::FilePath& install_dir) override;
  void OnCustomUninstall() override;
  bool VerifyInstallation(const base::Value::Dict& manifest,
                          const base::FilePath& install_dir) const override;
  void ComponentReady(const base::Version& version,
                      const base::FilePath& install_dir,
                      base::Value::Dict manifest) override;
  base::FilePath GetRelativeInstallDir() const override;
  void GetHash(std::vector<uint8_t>* hash) const override;
  std::string GetName() const override;
  update_client::InstallerAttributes GetInstallerAttributes() const override;
};

PnaclComponentInstallerPolicy::PnaclComponentInstallerPolicy() = default;
PnaclComponentInstallerPolicy::~PnaclComponentInstallerPolicy() = default;

bool PnaclComponentInstallerPolicy::SupportsGroupPolicyEnabledComponentUpdates()
    const {
  return true;
}

bool PnaclComponentInstallerPolicy::RequiresNetworkEncryption() const {
  return false;
}

update_client::CrxInstaller::Result
PnaclComponentInstallerPolicy::OnCustomInstall(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(0);  // Nothing custom here.
}

void PnaclComponentInstallerPolicy::OnCustomUninstall() {}

bool PnaclComponentInstallerPolicy::VerifyInstallation(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) const {
  if (std::optional<base::Value::Dict> pnacl_manifest =
          ReadPnaclManifest(install_dir)) {
    return CheckPnaclComponentManifest(manifest, *pnacl_manifest);
  } else {
    LOG(WARNING) << "Failed to read pnacl manifest.";
    return false;
  }
}

void PnaclComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    base::Value::Dict manifest) {
  CheckVersionCompatiblity(version);
  base::ThreadPool::PostTask(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(&OverrideDirPnaclComponent, install_dir));
}

base::FilePath PnaclComponentInstallerPolicy::GetRelativeInstallDir() const {
  return base::FilePath(FILE_PATH_LITERAL("pnacl"));
}
void PnaclComponentInstallerPolicy::GetHash(std::vector<uint8_t>* hash) const {
  hash->assign(std::begin(kPnaclPublicKeySHA256),
               std::end(kPnaclPublicKeySHA256));
}

std::string PnaclComponentInstallerPolicy::GetName() const {
  return "pnacl";
}

update_client::InstallerAttributes
PnaclComponentInstallerPolicy::GetInstallerAttributes() const {
  return update_client::InstallerAttributes();
}

}  // namespace

void RegisterPnaclComponent(ComponentUpdateService* cus) {
  // |cus| takes ownership of |installer| through the CrxComponent instance.
  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<PnaclComponentInstallerPolicy>());
  installer->Register(cus, base::OnceClosure());
}

void DeletePnaclComponent(const base::FilePath& user_data_dir) {
  base::ThreadPool::PostTask(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(base::IgnoreResult(&base::DeletePathRecursively),
                     user_data_dir.Append(FILE_PATH_LITERAL("pnacl"))));
}

}  // namespace component_updater

namespace pnacl {

bool NeedsOnDemandUpdate() {
  return base::subtle::NoBarrier_Load(
             &component_updater::needs_on_demand_update) != 0;
}

}  // namespace pnacl
