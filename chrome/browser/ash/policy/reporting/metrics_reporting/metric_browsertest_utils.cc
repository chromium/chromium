// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/metric_browsertest_utils.h"

#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/chromeos/reporting/metric_default_utils.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"

namespace ash::reporting {

MetricTestInitializationHelper::MetricTestInitializationHelper(
    ash::DeviceStateMixin* device_state)
    : device_state_(device_state) {
  device_state_->set_skip_initial_policy_setup(true);
}

MetricTestInitializationHelper::~MetricTestInitializationHelper() = default;

void MetricTestInitializationHelper::OnCoreConnected(
    policy::CloudPolicyCore* core) {
  run_loop_->Quit();
  mock_task_runner_ =
      std::make_unique<base::ScopedMockTimeMessageLoopTaskRunner>();
}

void MetricTestInitializationHelper::OnRefreshSchedulerStarted(
    policy::CloudPolicyCore* core) {}

void MetricTestInitializationHelper::OnCoreDisconnecting(
    policy::CloudPolicyCore* core) {}

void MetricTestInitializationHelper::SetUpDelayedInitialization() {
  auto* browser_policy_manager = g_browser_process->platform_part()
                                     ->browser_policy_connector_ash()
                                     ->GetDeviceCloudPolicyManager();
  run_loop_ = std::make_unique<base::RunLoop>();
  browser_policy_manager->core()->AddObserver(this);
  device_state_->RequestDevicePolicyUpdate();
  run_loop_->Run();
  mock_task_runner_->task_runner()->FastForwardBy(
      ::reporting::metrics::InitDelayParam::Get());
  browser_policy_manager->core()->RemoveObserver(this);
  // Destroy the mock task runner. The test can create its own
  // ScopedMockTimeMessageLoopTaskRunner if it requires further time control.
  mock_task_runner_.reset();
}

}  // namespace ash::reporting
