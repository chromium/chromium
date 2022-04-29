// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_URL_PARAM_CLASSIFICATION_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_URL_PARAM_CLASSIFICATION_COMPONENT_INSTALLER_H_

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/values.h"
#include "components/component_updater/component_installer.h"

namespace component_updater {

class ComponentUpdateService;

using OnUrlParamClassificationComponentReady =
    base::RepeatingCallback<void(std::string)>;

class UrlParamClassificationComponentInstallerPolicy
    : public ComponentInstallerPolicy {
 public:
  // The result of reading and validating the UrlParamFilterClassification list
  // from Component Updater.
  //
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class ClassificationListValidationResult {
    // No invalid classifications were found in the list.
    kSuccessful = 0,
    // The file wasn't present.
    kMissingClassificationsFile = 1,
    // Reading from the classifications file failed.
    kReadingClassificationsFileFailed = 2,
    // The raw classifications string was unabled to be parsed into the proto.
    kParsingToProtoFailed = 3,
    // Classification was ignored due to missing required site name.
    kClassificationMissingSite = 4,
    // Classification was ignored due to missing required site role.
    kClassificationMissingSiteRole = 5,
    kMaxValue = kClassificationMissingSiteRole,
  };

  explicit UrlParamClassificationComponentInstallerPolicy(
      OnUrlParamClassificationComponentReady on_component_ready);
  ~UrlParamClassificationComponentInstallerPolicy() override;

  UrlParamClassificationComponentInstallerPolicy(
      const UrlParamClassificationComponentInstallerPolicy&) = delete;
  UrlParamClassificationComponentInstallerPolicy& operator=(
      const UrlParamClassificationComponentInstallerPolicy&) = delete;

  static void WriteComponentForTesting(const base::FilePath& install_dir,
                                       base::StringPiece contents);
  static void ResetForTesting();

 private:
  FRIEND_TEST_ALL_PREFIXES(UrlParamClassificationComponentInstallerTest,
                           VerifyAttributes);

  // The following methods override ComponentInstallerPolicy.
  bool SupportsGroupPolicyEnabledComponentUpdates() const override;
  bool RequiresNetworkEncryption() const override;
  update_client::CrxInstaller::Result OnCustomInstall(
      const base::Value& manifest,
      const base::FilePath& install_dir) override;
  void OnCustomUninstall() override;
  bool VerifyInstallation(const base::Value& manifest,
                          const base::FilePath& install_dir) const override;
  void ComponentReady(const base::Version& version,
                      const base::FilePath& install_dir,
                      base::Value manifest) override;
  base::FilePath GetRelativeInstallDir() const override;
  void GetHash(std::vector<uint8_t>* hash) const override;
  std::string GetName() const override;
  update_client::InstallerAttributes GetInstallerAttributes() const override;

  static base::FilePath GetInstalledPath(const base::FilePath& base);
  void MaybeFireCallback(
      const absl::optional<std::string>& maybe_classifications);

  OnUrlParamClassificationComponentReady on_component_ready_;
};

// Call once during startup to make the component update service aware of
// the Url Param Classification component.
void RegisterUrlParamClassificationComponent(ComponentUpdateService* cus);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_URL_PARAM_CLASSIFICATION_COMPONENT_INSTALLER_H_
