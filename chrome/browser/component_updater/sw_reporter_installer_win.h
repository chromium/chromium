// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_SW_REPORTER_INSTALLER_WIN_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_SW_REPORTER_INSTALLER_WIN_H_

#include <stdint.h>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/sw_reporter_invocation_win.h"
#include "components/component_updater/component_installer.h"
#include "components/component_updater/component_updater_service.h"

class PrefRegistrySimple;

namespace base {
class DictionaryValue;
class FilePath;
class Version;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace component_updater {

class ComponentUpdateService;

constexpr char kSwReporterComponentId[] = "gkmgaooipdjhmangpemjhigmamcehddo";

// These MUST match the values for SoftwareReporterExperimentError in
// histograms.xml. Exposed for testing.
enum SoftwareReporterExperimentError {
  SW_REPORTER_EXPERIMENT_ERROR_BAD_TAG = 0,
  SW_REPORTER_EXPERIMENT_ERROR_BAD_PARAMS = 1,
  SW_REPORTER_EXPERIMENT_ERROR_MISSING_PARAMS = 2,
  SW_REPORTER_EXPERIMENT_ERROR_MAX,
};

// Callback for running the software reporter after it is downloaded.
using OnComponentReadyCallback = base::Callback<void(
    safe_browsing::SwReporterInvocationSequence&& invocations)>;

class SwReporterInstallerPolicy : public ComponentInstallerPolicy {
 public:
  explicit SwReporterInstallerPolicy(const OnComponentReadyCallback& callback);
  ~SwReporterInstallerPolicy() override;

  // ComponentInstallerPolicy implementation.
  bool VerifyInstallation(const base::DictionaryValue& manifest,
                          const base::FilePath& dir) const override;
  bool SupportsGroupPolicyEnabledComponentUpdates() const override;
  bool RequiresNetworkEncryption() const override;
  update_client::CrxInstaller::Result OnCustomInstall(
      const base::DictionaryValue& manifest,
      const base::FilePath& install_dir) override;
  void OnCustomUninstall() override;
  void ComponentReady(const base::Version& version,
                      const base::FilePath& install_dir,
                      std::unique_ptr<base::DictionaryValue> manifest) override;
  base::FilePath GetRelativeInstallDir() const override;
  void GetHash(std::vector<uint8_t>* hash) const override;
  std::string GetName() const override;
  update_client::InstallerAttributes GetInstallerAttributes() const override;
  std::vector<std::string> GetMimeTypes() const override;

 private:
  friend class SwReporterInstallerTest;

  OnComponentReadyCallback on_component_ready_callback_;

  DISALLOW_COPY_AND_ASSIGN(SwReporterInstallerPolicy);
};

// Forces an update of the reporter component.
// Note: this can have adverse effects on the component updater subsystem and
// should only be created as a result of direct user action.
// For example, if this is created repeatedly, it might result in too many
// unexpected requests to the component updater service and cause system
// instability.
class SwReporterOnDemandFetcher : public ServiceObserver {
 public:
  SwReporterOnDemandFetcher(ComponentUpdateService* cus,
                            base::OnceClosure on_error_callback);
  ~SwReporterOnDemandFetcher() override;

  // ServiceObserver implementation.
  void OnEvent(Events event, const std::string& id) override;

 private:
  // Will outlive this object.
  ComponentUpdateService* cus_;
  base::OnceClosure on_error_callback_;

  DISALLOW_COPY_AND_ASSIGN(SwReporterOnDemandFetcher);
};

// Call once during startup to make the component update service aware of the
// SwReporter. Once ready, this may trigger a periodic run of the reporter.
void RegisterSwReporterComponent(ComponentUpdateService* cus);

// Allow tests to register a function to be called when the registration
// of the reporter component is done.
void SetRegisterSwReporterComponentCallbackForTesting(
    base::OnceClosure registration_cb);

// Register local state preferences related to the SwReporter.
void RegisterPrefsForSwReporter(PrefRegistrySimple* registry);

// Register profile preferences related to the SwReporter.
void RegisterProfilePrefsForSwReporter(
    user_prefs::PrefRegistrySyncable* registry);

// Checks if we have information from the Cleaner and records UMA statistics.
void ReportUMAForLastCleanerRun();

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_SW_REPORTER_INSTALLER_WIN_H_
