// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/subresource_filter_component_installer.h"

#include <optional>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/browser_process.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/subresource_filter/content/shared/browser/ruleset_service.h"
#include "components/subresource_filter/core/browser/subresource_filter_constants.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/subresource_filter/core/common/constants.h"

using component_updater::ComponentUpdateService;

namespace component_updater {

// The extension id is: gcmjkmgdlgnkkcocmoeiminaijmmjnii
const uint8_t kSubresourceFilterPublicKeySHA256[32] = {
    0x62, 0xc9, 0xac, 0x63, 0xb6, 0xda, 0xa2, 0xe2, 0xce, 0x48, 0xc8,
    0xd0, 0x89, 0xcc, 0x9d, 0x88, 0x02, 0x7c, 0x3e, 0x71, 0xcf, 0x5d,
    0x6b, 0xb5, 0xdf, 0x21, 0x65, 0x82, 0x08, 0x97, 0x6a, 0x26};

const char kSubresourceFilterSetFetcherManifestName[] =
    "Subresource Filter Rules";

// static
const char
    SubresourceFilterComponentInstallerPolicy::kManifestRulesetFormatKey[] =
        "ruleset_format";

// static
const int SubresourceFilterComponentInstallerPolicy::kCurrentRulesetFormat = 1;

SubresourceFilterComponentInstallerPolicy::
    SubresourceFilterComponentInstallerPolicy() = default;

SubresourceFilterComponentInstallerPolicy::
    ~SubresourceFilterComponentInstallerPolicy() = default;

bool SubresourceFilterComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return false;
}

// Public data is delivered via this component, no need for encryption.
bool SubresourceFilterComponentInstallerPolicy::RequiresNetworkEncryption()
    const {
  return false;
}

update_client::CrxInstaller::Result
SubresourceFilterComponentInstallerPolicy::OnCustomInstall(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(0);  // Nothing custom here.
}

void SubresourceFilterComponentInstallerPolicy::OnCustomUninstall() {}

void SubresourceFilterComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    base::Value::Dict manifest) {
  DCHECK(!install_dir.empty());
  DVLOG(1) << "Subresource Filter Version Ready: " << install_dir.value();
  std::optional<int> ruleset_format =
      manifest.FindInt(kManifestRulesetFormatKey);
  if (!ruleset_format || *ruleset_format != kCurrentRulesetFormat) {
    DVLOG(1) << "Bailing out.";
    DVLOG_IF(1, ruleset_format)
        << "Future ruleset version: " << *ruleset_format;
    return;
  }
  subresource_filter::UnindexedRulesetInfo ruleset_info;
  ruleset_info.content_version = version.GetString();
  ruleset_info.ruleset_path =
      install_dir.Append(subresource_filter::kUnindexedRulesetDataFileName);
  ruleset_info.license_path =
      install_dir.Append(subresource_filter::kUnindexedRulesetLicenseFileName);
  subresource_filter::RulesetService* ruleset_service =
      g_browser_process->subresource_filter_ruleset_service();
  if (ruleset_service) {
    ruleset_service->IndexAndStoreAndPublishRulesetIfNeeded(ruleset_info);
  }
}

// Called during startup and installation before ComponentReady().
bool SubresourceFilterComponentInstallerPolicy::VerifyInstallation(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) const {
  return base::PathExists(install_dir);
}

base::FilePath
SubresourceFilterComponentInstallerPolicy::GetRelativeInstallDir() const {
  return base::FilePath(
             subresource_filter::kSafeBrowsingRulesetConfig.top_level_directory)
      .Append(subresource_filter::kUnindexedRulesetBaseDirectoryName);
}

void SubresourceFilterComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign(std::begin(kSubresourceFilterPublicKeySHA256),
               std::end(kSubresourceFilterPublicKeySHA256));
}

std::string SubresourceFilterComponentInstallerPolicy::GetName() const {
  return kSubresourceFilterSetFetcherManifestName;
}

// static
std::string SubresourceFilterComponentInstallerPolicy::GetInstallerTag() {
  const std::string ruleset_flavor(
      subresource_filter::GetEnabledConfigurations()
          ->lexicographically_greatest_ruleset_flavor());

  // Allow the empty, and 4 non-empty ruleset flavor identifiers: a, b, c, d.
  if (ruleset_flavor.empty()) {
    return ruleset_flavor;
  }

  if (ruleset_flavor.size() == 1 && ruleset_flavor.at(0) >= 'a' &&
      ruleset_flavor.at(0) <= 'd') {
    return ruleset_flavor;
  }

  // Return 'invalid' for any cases where we encounter an invalid installer
  // tag. This allows us to verify that no clients are encountering invalid
  // installer tags in the field.
  return "invalid";
}

update_client::InstallerAttributes
SubresourceFilterComponentInstallerPolicy::GetInstallerAttributes() const {
  update_client::InstallerAttributes attributes;
  std::string installer_tag = GetInstallerTag();
  if (!installer_tag.empty()) {
    attributes["tag"] = installer_tag;
  }
  return attributes;
}

void RegisterSubresourceFilterComponent(ComponentUpdateService* cus) {
  if (!base::FeatureList::IsEnabled(
          subresource_filter::kSafeBrowsingSubresourceFilter)) {
    return;
  }

  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<SubresourceFilterComponentInstallerPolicy>());
  installer->Register(cus, base::OnceClosure());
}

}  // namespace component_updater
