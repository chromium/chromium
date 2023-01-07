// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_SW_REPORTER_INSTALLER_WIN_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_SW_REPORTER_INSTALLER_WIN_H_

#include <stdint.h>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/sw_reporter_invocation_win.h"
#include "components/component_updater/component_installer.h"
#include "components/component_updater/component_updater_service.h"

class PrefRegistrySimple;
class PrefService;

namespace base {
class FilePath;
class Version;
}  // namespace base

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace component_updater {

class ComponentUpdateService;

constexpr char kSwReporterComponentId[] = "gkmgaooipdjhmangpemjhigmamcehddo";

// These values are logged to UMA. Entries should not be renumbered and
// numeric values should never be reused. Please keep in sync with
// "SoftwareReporterConfigurationError" in
// src/tools/metrics/histograms/enums.xml.
enum SoftwareReporterConfigurationError : uint8_t {
  kBadTag = 0,
  kBadParams = 1,
  kMissingParams = 2,
  kMissingPromptSeed = 3,
  kMaxValue = kMissingPromptSeed
};

// Callback for running the software reporter after it is downloaded.
using OnComponentReadyCallback = base::RepeatingCallback<void(
    const std::string& prompt_seed,
    safe_browsing::SwReporterInvocationSequence&& invocations)>;

class SwReporterInstallerPolicy : public ComponentInstallerPolicy {
 public:
  SwReporterInstallerPolicy(PrefService* prefs,
                            OnComponentReadyCallback callback);

  SwReporterInstallerPolicy(const SwReporterInstallerPolicy&) = delete;
  SwReporterInstallerPolicy& operator=(const SwReporterInstallerPolicy&) =
      delete;

  ~SwReporterInstallerPolicy() override;

  // Use `cohort_name` instead of a random reporter cohort tag if no tag is set
  // in a feature param or prefs. In production the policy randomly chooses
  // "canary" or "stable".
  void SetRandomReporterCohortForTesting(const std::string& cohort_name);

  // ComponentInstallerPolicy implementation.
  bool VerifyInstallation(const base::Value::Dict& manifest,
                          const base::FilePath& dir) const override;
  bool SupportsGroupPolicyEnabledComponentUpdates() const override;
  bool RequiresNetworkEncryption() const override;
  update_client::CrxInstaller::Result OnCustomInstall(
      const base::Value::Dict& manifest,
      const base::FilePath& install_dir) override;
  void OnCustomUninstall() override;
  void ComponentReady(const base::Version& version,
                      const base::FilePath& install_dir,
                      base::Value::Dict manifest) override;
  base::FilePath GetRelativeInstallDir() const override;
  void GetHash(std::vector<uint8_t>* hash) const override;
  std::string GetName() const override;
  update_client::InstallerAttributes GetInstallerAttributes() const override;

 private:
  friend class SwReporterInstallerTest;

  // Returns a value for the "tag" param.
  std::string GetReporterCohortTag(PrefService* prefs) const;

  raw_ptr<PrefService> prefs_;
  OnComponentReadyCallback on_component_ready_callback_;

  // This string will be used as the "random" cohort name in tests.
  std::string random_cohort_for_testing_;
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

  SwReporterOnDemandFetcher(const SwReporterOnDemandFetcher&) = delete;
  SwReporterOnDemandFetcher& operator=(const SwReporterOnDemandFetcher&) =
      delete;

  ~SwReporterOnDemandFetcher() override;

  // ServiceObserver implementation.
  void OnEvent(Events event, const std::string& id) override;

 private:
  // Will outlive this object.
  raw_ptr<ComponentUpdateService> cus_;
  base::OnceClosure on_error_callback_;
};

// Call once during startup to make the component update service aware of the
// SwReporter. Once ready, this may trigger a periodic run of the reporter.
void RegisterSwReporterComponent(ComponentUpdateService* cus,
                                 PrefService* prefs);

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
