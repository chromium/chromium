// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/metric_browsertest_utils.h"

#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/chromeos/reporting/metric_default_utils.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"

namespace ash::reporting {

MetricBrowserTestBase::MetricBrowserTestBase() {
  device_state_.set_skip_initial_policy_setup(true);
}

MetricBrowserTestBase::~MetricBrowserTestBase() = default;

void MetricBrowserTestBase::OnCoreConnected(policy::CloudPolicyCore* core) {
  run_loop_->Quit();
  mock_task_runner_ =
      std::make_unique<base::ScopedMockTimeMessageLoopTaskRunner>();
}

void MetricBrowserTestBase::OnRefreshSchedulerStarted(
    policy::CloudPolicyCore* core) {}

void MetricBrowserTestBase::OnCoreDisconnecting(policy::CloudPolicyCore* core) {
}

void MetricBrowserTestBase::SetUpDelayedInitialization() {
  auto* browser_policy_manager = g_browser_process->platform_part()
                                     ->browser_policy_connector_ash()
                                     ->GetDeviceCloudPolicyManager();
  run_loop_ = std::make_unique<base::RunLoop>();
  browser_policy_manager->core()->AddObserver(this);
  device_state_.RequestDevicePolicyUpdate();
  run_loop_->Run();
  mock_task_runner_->task_runner()->FastForwardBy(
      ::reporting::metrics::kInitDelay);
  browser_policy_manager->core()->RemoveObserver(this);
  // Destroy the mock task runner. The test can create its own
  // ScopedMockTimeMessageLoopTaskRunner if it requires further time control.
  mock_task_runner_.reset();
}

}  // namespace ash::reporting
