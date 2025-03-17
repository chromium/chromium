// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_CROS_PRE_CONSENT_METRICS_MANAGER_H_
#define CHROME_BROWSER_METRICS_CROS_PRE_CONSENT_METRICS_MANAGER_H_

#include <memory>
#include <optional>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_manager_ash.h"

class PrefService;
class PrefRegistrySimple;

namespace base {
class SequencedTaskRunner;
}

namespace metrics {

class MetricsServiceClient;

// Manages the setting of metrics during ChromeOS OOBE before the primary user
// has been created.
//
// Primary functionality is to facilitate enabling metrics in desired
// circumstances, such as:
// * The primary user hasn't been set.
// * The primary user hasn't consented or dissented to metrics collection.
// * The device is restarted and any of the above is true.
//
// This object is disabled during OOBE when the metrics is set by primary user
// (regular, demo, or guest) or by policy.
// * Regular user: Disabled when user accepts setting on the Consolidated
// Consent Screen.
// * Demo User: Disabled when user accepts setting on the Consolidated
// Consent Screen.
// * Guest User: Disabled when the Guest ToS screen is accepted.
// * Managed User: Disabled during when the Consolidated Consent Screen is
// skipped due to managed user.
class CrOSPreConsentMetricsManager
    : public policy::CloudPolicyStore::Observer {
 public:
  ~CrOSPreConsentMetricsManager() override;

  // Enables pre-consent metrics. This will force metrics to be enabled and
  // metrics will be uploaded.
  void Enable();

  // Disables pre-consent metrics. This will write a marker file to signify
  // that the primary user has given their metrics consent.
  void Disable();

  // The upload interval to use while enabled.
  std::optional<base::TimeDelta> GetUploadInterval() const;

  // Access to the inner state for testing.
  bool is_enabled_for_testing() const { return is_enabled_; }

  // Sets a path to write when manager is disabled for testing.
  void SetCompletedPathForTesting(const base::FilePath& path);

  // Posts a task to |task_runner_| to handle async operations during testing.
  //
  // Primary use case is:
  // PostToIOTaskRunnerForTesting(FROM_HERE, run_loop.QuitClosure());
  void PostToIOTaskRunnerForTesting(base::Location here,
                                    base::OnceClosure callback);

  // Gets the singleton instance.
  static CrOSPreConsentMetricsManager* Get();

  // Conditionally creates a new instance of CrOSPreConsentMetricsManager
  // depending on |ash::feature::kOobePreConsentMetrics| feature or
  // the existence of a marker file.
  static std::unique_ptr<CrOSPreConsentMetricsManager> MaybeCreate();

 private:
  CrOSPreConsentMetricsManager();

  // policy::CloudPolicyStore::Observer interface:
  void OnStoreError(policy::CloudPolicyStore* store) override;
  void OnStoreLoaded(policy::CloudPolicyStore* store) override;

  // Flag for whether this functionality is enabled.
  bool is_enabled_ = false;

  // Task runner for creating the completed file.
  //
  // This is a dedicated task runner to allow for easier testing.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // A path used to test the Disable functionality.
  std::optional<base::FilePath> completed_path_for_testing_;


  base::ScopedObservation<policy::DeviceCloudPolicyStoreAsh,
                          CrOSPreConsentMetricsManager>
      cloud_policy_store_observation_{this};
};

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_CROS_PRE_CONSENT_METRICS_MANAGER_H_
