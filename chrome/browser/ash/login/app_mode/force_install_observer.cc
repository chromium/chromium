// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/app_mode/force_install_observer.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/syslog_logging.h"
#include "base/time/time.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/force_installed_tracker_ash.h"
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

namespace {

// Time of waiting for the force-installed extension to be ready to start
// application window. Can be changed in tests.
constexpr base::TimeDelta kKioskExtensionWaitTime = base::Minutes(2);
base::TimeDelta g_installation_wait_time = kKioskExtensionWaitTime;

extensions::ForceInstalledTracker* GetForceInstalledTracker(Profile* profile) {
  extensions::ExtensionSystem* system =
      extensions::ExtensionSystem::Get(profile);
  DCHECK(system);

  extensions::ExtensionService* service = system->extension_service();
  return service ? service->force_installed_tracker() : nullptr;
}

crosapi::ForceInstalledTrackerAsh* GetForceInstalledTrackerAsh() {
  CHECK(crosapi::CrosapiManager::IsInitialized());
  return crosapi::CrosapiManager::Get()
      ->crosapi_ash()
      ->force_installed_tracker_ash();
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

void RecordKioskExtensionInstallError(
    extensions::InstallStageTracker::FailureReason reason,
    bool is_from_store) {
  if (is_from_store) {
    base::UmaHistogramEnumeration("Kiosk.Extensions.InstallError.WebStore",
                                  reason);
  } else {
    base::UmaHistogramEnumeration("Kiosk.Extensions.InstallError.OffStore",
                                  reason);
  }
}

void RecordKioskExtensionInstallDuration(base::TimeDelta time_delta) {
  UMA_HISTOGRAM_MEDIUM_TIMES("Kiosk.Extensions.InstallDuration", time_delta);
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

  if (crosapi::browser_util::IsLacrosEnabledInWebKioskSession() ||
      crosapi::browser_util::IsLacrosEnabledInChromeKioskSession()) {
    StartObservingLacros();
  } else {
    StartObservingAsh(profile);
  }
}

ForceInstallObserver::~ForceInstallObserver() = default;

void ForceInstallObserver::StartObservingAsh(Profile* profile) {
  extensions::ForceInstalledTracker* tracker =
      GetForceInstalledTracker(profile);
  if (tracker && !tracker->IsReady()) {
    observation_for_ash_.Observe(tracker);
    StartTimerToWaitForExtensions();
  } else {
    ReportDone();
  }
}

void ForceInstallObserver::StartObservingLacros() {
  crosapi::ForceInstalledTrackerAsh* tracker = GetForceInstalledTrackerAsh();
  if (tracker && !tracker->IsReady()) {
    observation_for_lacros_.Observe(tracker);
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
  SYSLOG(WARNING) << "OnExtensionWaitTimeout...";

  RecordKioskExtensionInstallDuration(base::Time::Now() -
                                      installation_start_time_);
  RecordKioskExtensionInstallTimedOut(true);
  ReportTimeout();
}

void ForceInstallObserver::OnForceInstalledExtensionsReady() {
  RecordKioskExtensionInstallDuration(base::Time::Now() -
                                      installation_start_time_);
  RecordKioskExtensionInstallTimedOut(false);
  ReportDone();
}

void ForceInstallObserver::OnForceInstalledExtensionFailed(
    const extensions::ExtensionId& installation_id,
    extensions::InstallStageTracker::FailureReason reason,
    bool is_from_store) {
  // We will still receive the OnForceInstalledExtensionsReady callback, so only
  // log this failure.
  RecordKioskExtensionInstallError(reason, is_from_store);
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
