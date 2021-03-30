// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/autofill_regex_component_installer.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/version.h"
#include "components/autofill/core/browser/pattern_provider/pattern_configuration_parser.h"
#include "components/component_updater/component_updater_paths.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

using component_updater::ComponentUpdateService;

namespace {

constexpr base::FilePath::CharType kAutofillRegexJsonFileName[] =
    FILE_PATH_LITERAL("data.json");

// The SHA256 of the SubjectPublicKeyInfo used to sign the extension.
// The extension id is: PDAFIOLLNGONHOADBMDOEMAGNFPDPHBE
constexpr uint8_t kAutofillRegexPublicKeySHA256[32] = {
    0xf3, 0x05, 0x8e, 0xbb, 0xd6, 0xed, 0x7e, 0x03, 0x1c, 0x3e, 0x4c,
    0x06, 0xd5, 0xf3, 0xf7, 0x14, 0x93, 0x75, 0xf4, 0x24, 0x67, 0x4e,
    0x61, 0x1e, 0xba, 0x5f, 0xf4, 0x29, 0x27, 0x0e, 0x1c, 0x1e};

constexpr char kAutofillRegexManifestName[] = "Autofill Regex Data";

void LoadRegexConfigurationFromDisk(const base::FilePath& json_path) {
  if (json_path.empty())
    return;

  VLOG(1) << "Reading Download File Types from file: " << json_path.value();
  std::string json_string;
  if (!base::ReadFileToString(json_path, &json_string)) {
    // The file won't exist on new installations, so this is not always an
    // error.
    VLOG(1) << "Failed reading from " << json_path.value();
    return;
  }

  content::GetUIThreadTaskRunner(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT})
      ->PostTask(
          FROM_HERE,
          base::BindOnce(&autofill::field_type_parsing::PopulateFromJsonString,
                         std::move(json_string)));
}

}  // namespace

namespace component_updater {

bool AutofillRegexComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return false;
}

bool AutofillRegexComponentInstallerPolicy::RequiresNetworkEncryption() const {
  return false;
}

update_client::CrxInstaller::Result
AutofillRegexComponentInstallerPolicy::OnCustomInstall(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(0);  // Nothing custom here.
}

void AutofillRegexComponentInstallerPolicy::OnCustomUninstall() {}

base::FilePath AutofillRegexComponentInstallerPolicy::GetInstalledPath(
    const base::FilePath& base) {
  return base.Append(kAutofillRegexJsonFileName);
}

void AutofillRegexComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    std::unique_ptr<base::DictionaryValue> manifest) {
  VLOG(1) << "Component ready, version " << version.GetString() << " in "
          << install_dir.value();

  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&LoadRegexConfigurationFromDisk,
                     GetInstalledPath(install_dir)));
}

// Called during startup and installation before ComponentReady().
bool AutofillRegexComponentInstallerPolicy::VerifyInstallation(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) const {
  // Not doing any validation, it will be done during parsing.
  return base::PathExists(GetInstalledPath(install_dir));
}

base::FilePath AutofillRegexComponentInstallerPolicy::GetRelativeInstallDir()
    const {
  return base::FilePath(FILE_PATH_LITERAL("AutofillRegex"));
}

void AutofillRegexComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign(std::begin(kAutofillRegexPublicKeySHA256),
               std::end(kAutofillRegexPublicKeySHA256));
}

std::string AutofillRegexComponentInstallerPolicy::GetName() const {
  return kAutofillRegexManifestName;
}

update_client::InstallerAttributes
AutofillRegexComponentInstallerPolicy::GetInstallerAttributes() const {
  return update_client::InstallerAttributes();
}

void RegisterAutofillRegexComponent(ComponentUpdateService* cus) {
  VLOG(1) << "Registering Autofill Regex component.";
  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<AutofillRegexComponentInstallerPolicy>());
  installer->Register(cus, base::OnceClosure());
}

}  // namespace component_updater
