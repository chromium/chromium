// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/afp_blocked_domain_list_component_installer.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/task/thread_pool.h"
#include "base/version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_features.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_filter_constants.h"
#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_filter_features.h"
#include "components/subresource_filter/content/shared/browser/ruleset_service.h"
#include "components/subresource_filter/core/browser/subresource_filter_constants.h"

using component_updater::ComponentUpdateService;

namespace component_updater {

// The SHA256 of the SubjectPublicKeyInfo used to sign the component.
// The CRX ID is: kgdbnmlfakkebekbaceapiaenjgmlhan.
const uint8_t kAfpBlockedDomainListPublicKeySHA256[32] = {
    0xa6, 0x31, 0xdc, 0xb5, 0x0a, 0xa4, 0x14, 0xa1, 0x02, 0x40, 0xf8,
    0x04, 0xd9, 0x6c, 0xb7, 0x0d, 0x7b, 0xbd, 0x63, 0xf9, 0xc8, 0x65,
    0x6e, 0x9b, 0x83, 0x7a, 0x3a, 0xfd, 0xd1, 0xc8, 0x40, 0xe3};

const char kAfpBlockedDomainListManifestName[] =
    "Fingerprinting Protection Filter Rules";

// static
const char AntiFingerprintingBlockedDomainListComponentInstallerPolicy::
    kManifestRulesetFormatKey[] = "ruleset_format";

// static
const int AntiFingerprintingBlockedDomainListComponentInstallerPolicy::
    kCurrentRulesetFormat = 1;

AntiFingerprintingBlockedDomainListComponentInstallerPolicy::
    AntiFingerprintingBlockedDomainListComponentInstallerPolicy() = default;

AntiFingerprintingBlockedDomainListComponentInstallerPolicy::
    ~AntiFingerprintingBlockedDomainListComponentInstallerPolicy() = default;

bool AntiFingerprintingBlockedDomainListComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  // Allow enterprise admins to disable updates to this component.
  return true;
}

bool AntiFingerprintingBlockedDomainListComponentInstallerPolicy::
    RequiresNetworkEncryption() const {
  // No encryption required since the blocklist will be public and identical for
  // all users.
  return false;
}

update_client::CrxInstaller::Result
AntiFingerprintingBlockedDomainListComponentInstallerPolicy::OnCustomInstall(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(0);  // Nothing custom here.
}

void AntiFingerprintingBlockedDomainListComponentInstallerPolicy::
    OnCustomUninstall() {}

void AntiFingerprintingBlockedDomainListComponentInstallerPolicy::
    ComponentReady(const base::Version& version,
                   const base::FilePath& install_dir,
                   base::Value::Dict manifest) {
  CHECK(!install_dir.empty());
  DVLOG(1)
      << "Anti-Fingerprinting Blocked Domain List Component ready, version "
      << version.GetString() << " in " << install_dir.value();
  subresource_filter::UnindexedRulesetInfo ruleset_info;
  ruleset_info.content_version = version.GetString();
  ruleset_info.ruleset_path = install_dir.Append(
      fingerprinting_protection_filter::kUnindexedRulesetDataFileName);
  ruleset_info.license_path =
      install_dir.Append(subresource_filter::kUnindexedRulesetLicenseFileName);
  subresource_filter::RulesetService* ruleset_service =
      g_browser_process->fingerprinting_protection_ruleset_service();
  if (ruleset_service != nullptr) {
    ruleset_service->IndexAndStoreAndPublishRulesetIfNeeded(ruleset_info);
  }
}

// Called during startup and installation before ComponentReady().
bool AntiFingerprintingBlockedDomainListComponentInstallerPolicy::
    VerifyInstallation(const base::Value::Dict& manifest,
                       const base::FilePath& install_dir) const {
  std::optional<int> ruleset_format =
      manifest.FindInt(kManifestRulesetFormatKey);
  if (!ruleset_format.has_value() || *ruleset_format != kCurrentRulesetFormat) {
    DVLOG(1) << "Ruleset formats don't match.";
    DVLOG_IF(1, ruleset_format)
        << "Future ruleset version: " << *ruleset_format;
    return false;
  }
  return base::PathExists(install_dir);
}

base::FilePath AntiFingerprintingBlockedDomainListComponentInstallerPolicy::
    GetRelativeInstallDir() const {
  return base::FilePath(
             fingerprinting_protection_filter::
                 kFingerprintingProtectionRulesetConfig.top_level_directory)
      .Append(subresource_filter::kUnindexedRulesetBaseDirectoryName);
}

void AntiFingerprintingBlockedDomainListComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign(std::begin(kAfpBlockedDomainListPublicKeySHA256),
               std::end(kAfpBlockedDomainListPublicKeySHA256));
}

std::string
AntiFingerprintingBlockedDomainListComponentInstallerPolicy::GetName() const {
  return kAfpBlockedDomainListManifestName;
}

update_client::InstallerAttributes
AntiFingerprintingBlockedDomainListComponentInstallerPolicy::
    GetInstallerAttributes() const {
  return update_client::InstallerAttributes();
}

void RegisterAntiFingerprintingBlockedDomainListComponent(
    ComponentUpdateService* cus) {
  if (!fingerprinting_protection_filter::features::
          IsFingerprintingProtectionFeatureEnabled()) {
    return;
  }

  VLOG(1) << "Registering Anti-Fingerprinting Blocked Domain List Component.";
  auto policy = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<
          AntiFingerprintingBlockedDomainListComponentInstallerPolicy>());
  policy->Register(cus, base::OnceClosure());
}

}  // namespace component_updater
