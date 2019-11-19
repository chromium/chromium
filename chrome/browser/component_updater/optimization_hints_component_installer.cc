// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/optimization_hints_component_installer.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/task/post_task.h"
#include "base/version.h"
#include "chrome/browser/browser_process.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/optimization_guide/optimization_guide_constants.h"
#include "components/optimization_guide/optimization_guide_features.h"
#include "components/optimization_guide/optimization_guide_service.h"
#include "components/prefs/pref_service.h"

using component_updater::ComponentUpdateService;

namespace component_updater {

namespace {

const char kDisableInstallerUpdate[] = "optimization-guide-disable-installer";

// The extension id is: lmelglejhemejginpboagddgdfbepgmp
const uint8_t kOptimizationHintsPublicKeySHA256[32] = {
    0xbc, 0x4b, 0x6b, 0x49, 0x74, 0xc4, 0x96, 0x8d, 0xf1, 0xe0, 0x63,
    0x36, 0x35, 0x14, 0xf6, 0xcf, 0x86, 0x92, 0xe6, 0x06, 0x03, 0x76,
    0x70, 0xaf, 0x8b, 0xd4, 0x47, 0x2c, 0x42, 0x59, 0x38, 0xef};

const char kOptimizationHintsSetFetcherManifestName[] = "Optimization Hints";

}  // namespace

// static
const char
    OptimizationHintsComponentInstallerPolicy::kManifestRulesetFormatKey[] =
        "ruleset_format";

OptimizationHintsComponentInstallerPolicy::
    OptimizationHintsComponentInstallerPolicy()
    : ruleset_format_version_(
          base::Version(optimization_guide::kRulesetFormatVersionString)) {
  DCHECK(ruleset_format_version_.IsValid());
}

OptimizationHintsComponentInstallerPolicy::
    ~OptimizationHintsComponentInstallerPolicy() {}

bool OptimizationHintsComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return false;
}

bool OptimizationHintsComponentInstallerPolicy::RequiresNetworkEncryption()
    const {
  return false;
}

update_client::CrxInstaller::Result
OptimizationHintsComponentInstallerPolicy::OnCustomInstall(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(0);  // Nothing custom here.
}

void OptimizationHintsComponentInstallerPolicy::OnCustomUninstall() {}

void OptimizationHintsComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    std::unique_ptr<base::DictionaryValue> manifest) {
  DCHECK(!install_dir.empty());
  DVLOG(1) << "Optimization Hints Version Ready: " << version.GetString();
  std::string ruleset_format;
  if (!manifest->GetString(kManifestRulesetFormatKey, &ruleset_format)) {
    DVLOG(1) << "No ruleset_format present in manifest";
    return;
  }
  base::Version ruleset_format_version = base::Version(ruleset_format);
  if (!ruleset_format_version.IsValid() ||
      ruleset_format_version.CompareTo(ruleset_format_version_) > 0) {
    DVLOG(1) << "Got incompatible ruleset_format. Bailing out.";
    return;
  }
  optimization_guide::OptimizationGuideService* optimization_guide_service =
      g_browser_process->optimization_guide_service();
  if (optimization_guide_service &&
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          kDisableInstallerUpdate)) {
    optimization_guide::HintsComponentInfo info(
        version,
        install_dir.Append(optimization_guide::kUnindexedHintsFileName));
    optimization_guide_service->MaybeUpdateHintsComponent(info);
  }
}

// Called during startup and installation before ComponentReady().
bool OptimizationHintsComponentInstallerPolicy::VerifyInstallation(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) const {
  return base::PathExists(install_dir);
}

base::FilePath
OptimizationHintsComponentInstallerPolicy::GetRelativeInstallDir() const {
  return base::FilePath(FILE_PATH_LITERAL("OptimizationHints"));
}

void OptimizationHintsComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  if (!hash) {
    return;
  }
  hash->assign(std::begin(kOptimizationHintsPublicKeySHA256),
               std::end(kOptimizationHintsPublicKeySHA256));
}

std::string OptimizationHintsComponentInstallerPolicy::GetName() const {
  return kOptimizationHintsSetFetcherManifestName;
}

update_client::InstallerAttributes
OptimizationHintsComponentInstallerPolicy::GetInstallerAttributes() const {
  return update_client::InstallerAttributes();
}

std::vector<std::string>
OptimizationHintsComponentInstallerPolicy::GetMimeTypes() const {
  return std::vector<std::string>();
}

void RegisterOptimizationHintsComponent(ComponentUpdateService* cus,
                                        bool is_off_the_record_profile,
                                        PrefService* profile_prefs) {
  if (!optimization_guide::features::IsOptimizationHintsEnabled()) {
    return;
  }

  if (!data_reduction_proxy::DataReductionProxySettings::
          IsDataSaverEnabledByUser(is_off_the_record_profile, profile_prefs)) {
    return;
  }
  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<OptimizationHintsComponentInstallerPolicy>());
  installer->Register(cus, base::OnceClosure());
}

}  // namespace component_updater
