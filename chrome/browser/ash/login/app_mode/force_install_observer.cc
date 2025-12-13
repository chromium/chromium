// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/app_mode/force_install_observer.h"

#include <string>
#include <utility>

#include "base/check.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/syslog_logging.h"
#include "base/time/time.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/forced_extensions/force_installed_tracker.h"
#include "chrome/browser/extensions/forced_extensions/install_stage_tracker.h"
#include "chrome/browser/extensions/policy_handlers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/policy_constants.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension_id.h"

namespace {

// Time of waiting for the force-installed extension to be ready to start
// application window. Can be changed in tests.
constexpr base::TimeDelta kKioskExtensionWaitTime = base::Minutes(2);
base::TimeDelta g_installation_wait_time = kKioskExtensionWaitTime;

extensions::ForceInstalledTracker* GetForceInstalledTracker(Profile* profile) {
  auto* system = extensions::ExtensionSystem::Get(profile);
  DCHECK(system);

  extensions::ExtensionService* service = system->extension_service();
  return service ? service->force_installed_tracker() : nullptr;
}

bool IsExtensionInstallForcelistPolicyValid() {
  policy::PolicyService* policy_service = g_browser_process->platform_part()
                                              ->browser_policy_connector_ash()
                                              ->GetPolicyService();
  DCHECK(policy_service);

  const policy::PolicyMap& map =
      policy_service->GetPolicies(policy::PolicyNamespace(
          policy::PolicyDomain::POLICY_DOMAIN_CHROME, std::string()));

  extensions::ExtensionInstallForceListPolicyHandler handler;
  policy::PolicyErrorMap errors;
  handler.CheckPolicySettings(map, &errors);
  return errors.GetErrors(policy::key::kExtensionInstallForcelist).empty();
}

void RecordKioskExtensionInstallTimedOut(bool timeout) {
  UMA_HISTOGRAM_BOOLEAN("Kiosk.Extensions.InstallTimedOut", timeout);
}

}  // namespace

namespace app_mode {

ForceInstallObserver::ForceInstallObserver(Profile* profile,
                                           ResultCallback callback)
    : callback_(std::move(callback)) {
  if (!IsExtensionInstallForcelistPolicyValid()) {
    SYSLOG(WARNING) << "The ExtensionInstallForcelist policy value is invalid.";
    ReportInvalidPolicy();
    return;
  }

  StartObserving(profile);
}

ForceInstallObserver::~ForceInstallObserver() = default;

void ForceInstallObserver::StartObserving(Profile* profile) {
  extensions::ForceInstalledTracker* tracker =
      GetForceInstalledTracker(profile);
  if (tracker && !tracker->IsReady()) {
    observation_.Observe(tracker);
    StartTimerToWaitForExtensions();
  } else {
    ReportDone();
  }
}

void ForceInstallObserver::StartTimerToWaitForExtensions() {
  installation_start_time_ = base::Time::Now();
  installation_wait_timer_.Start(FROM_HERE, g_installation_wait_time, this,
                                 &ForceInstallObserver::OnExtensionWaitTimeOut);
}

void ForceInstallObserver::OnExtensionWaitTimeOut() {
  SYSLOG(WARNING) << "Timed out waiting for extensions to install";

  RecordKioskExtensionInstallTimedOut(true);
  ReportTimeout();
}

void ForceInstallObserver::OnForceInstalledExtensionsReady() {
  RecordKioskExtensionInstallTimedOut(false);
  ReportDone();
}

void ForceInstallObserver::ReportDone() {
  std::move(callback_).Run(Result::kSuccess);
}

void ForceInstallObserver::ReportTimeout() {
  std::move(callback_).Run(Result::kTimeout);
}

void ForceInstallObserver::ReportInvalidPolicy() {
  std::move(callback_).Run(Result::kInvalidPolicy);
}

}  // namespace app_mode
