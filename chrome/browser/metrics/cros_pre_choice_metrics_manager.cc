// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/cros_pre_choice_metrics_manager.h"

#include <optional>

#include "ash/constants/ash_features.h"
#include "base/check.h"
#include "base/check_deref.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_store_ash.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_ash.h"
#include "chrome/browser/metrics/metrics_reporting_state.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service_client.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace metrics {

// Singleton pointer to the instance of CrOSPreChoiceMetricsManager.
static CrOSPreChoiceMetricsManager* g_instance = nullptr;

// Path for pre_consent_complete file. This file is created when the pre-choice
// is considered completed depending on the type of user.
//
// See class comment on CrOSPreChoiceMetricsManager for details on when this
// file is written.
const char kCrOSPreConsentCompletePath[] =
    "/home/chronos/.pre_consent_complete";

// The upload interval for metrics during pre-choice metrics.
const base::TimeDelta kPreChoiceUploadInterval = base::Seconds(120);

// Writes the .pre_consent_complete file in chronos home. This signifies that
// the primary user has been set and their choice for metrics has been set.
void WritePreConsentCompleteFile(std::optional<base::FilePath> test_path) {
  base::FilePath path = base::FilePath(kCrOSPreConsentCompletePath);
  if (test_path.has_value()) {
    path = std::move(test_path).value();
  }
  base::WriteFile(path, "");
}

// static
std::unique_ptr<CrOSPreChoiceMetricsManager>
CrOSPreChoiceMetricsManager::MaybeCreate() {
  // If this path exists then this object doesn't need to be created.
  if (!ash::features::IsOobePreConsentMetricsEnabled() ||
      base::PathExists(base::FilePath(kCrOSPreConsentCompletePath))) {
    return nullptr;
  }

  // Handle when a ChromeOS device upgrades to the version this functionality
  // was added that has completed OOBE.
  if (ash::StartupUtils::IsDeviceRegistered(
          CHECK_DEREF(g_browser_process->local_state()))) {
    base::ThreadPool::PostTask(
        FROM_HERE, base::MayBlock(),
        base::BindOnce(&WritePreConsentCompleteFile,
                       base::FilePath(kCrOSPreConsentCompletePath)));
    return nullptr;
  }

  return base::WrapUnique(new CrOSPreChoiceMetricsManager());
}

CrOSPreChoiceMetricsManager::~CrOSPreChoiceMetricsManager() {
  g_instance = nullptr;
}

void CrOSPreChoiceMetricsManager::Enable() {
  if (is_enabled_) {
    return;
  }

  is_enabled_ = true;
  VLOG(1) << "Pre-choice metrics enabled";

  // Force enable metrics. This will enable metrics and populate all appropriate
  // preferences.
  metrics::ChangeMetricsReportingState(
      metrics::MetricsReportingLevel::kBasic,
      metrics::ChangeMetricsReportingStateCalledFrom::kCrosMetricsPreConsent);

  // Propagate the change to metrics services. This will create the Client ID
  // that will be used if the user consents to metrics. If pre-choice is being
  // disabled do not update the permissions as it should not be changed.
  g_browser_process->GetMetricsServicesManager()->UpdateUploadPermissions();

  // Register CrOSPreChoiceMetricsManager as the observer for policy change to
  // get notified when device is enrolled.
  cloud_policy_store_observation_.Observe(g_browser_process->platform_part()
                                              ->browser_policy_connector_ash()
                                              ->GetDeviceCloudPolicyManager()
                                              ->device_store());
}

void CrOSPreChoiceMetricsManager::Disable() {
  if (!is_enabled_) {
    return;
  }

  is_enabled_ = false;
  VLOG(1) << "Pre-choice metrics disabled";

  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&WritePreConsentCompleteFile,
                                        completed_path_for_testing_));
}

std::optional<base::TimeDelta> CrOSPreChoiceMetricsManager::GetUploadInterval()
    const {
  if (is_enabled_) {
    return kPreChoiceUploadInterval;
  }
  return std::nullopt;
}

void CrOSPreChoiceMetricsManager::SetCompletedPathForTesting(  // IN-TEST
    const base::FilePath& path) {
  completed_path_for_testing_ = path;
}

void CrOSPreChoiceMetricsManager::PostToIOTaskRunnerForTesting(  // IN-TEST
    base::Location here,
    base::OnceClosure callback) {
  task_runner_->PostTask(here, std::move(callback));
}

// static
CrOSPreChoiceMetricsManager* CrOSPreChoiceMetricsManager::Get() {
  return g_instance;
}

CrOSPreChoiceMetricsManager::CrOSPreChoiceMetricsManager()
    : task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner(base::MayBlock())) {
  CHECK(g_instance == nullptr) << "CrOSPreChoiceMetricsManager already exists";
  g_instance = this;
}

void CrOSPreChoiceMetricsManager::OnStoreError(
    policy::CloudPolicyStore* store) {}

void CrOSPreChoiceMetricsManager::OnStoreLoaded(
    policy::CloudPolicyStore* store) {
  cloud_policy_store_observation_.Reset();

  Disable();
}

}  // namespace metrics
