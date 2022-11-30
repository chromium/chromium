// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_METRIC_BROWSERTEST_UTILS_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_METRIC_BROWSERTEST_UTILS_H_

#include "base/test/scoped_mock_time_message_loop_task_runner.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "content/public/test/browser_test.h"

namespace ash::reporting {

// A utility that provides initialization support for metric browser tests.

// One main challenge of the metric browser tests is the delayed initialization
// (which enqueues info metric) by |MetricReportingManager|: It must be called
// after |MissiveClientTestObserver| has been set up (so setting delayed
// initialization time to zero won't work) but before the rest of the test body.
// Therefore, some mock time manipulation is required. Due to the complexity of
// mocking time in browser tests, this class addresses the issue as follows:
//
// - In its constructor, initial policy setup is skipped. This prevents delayed
// initialization task from being posted.
// - A method |SetUpDelayedInitialization| is provided. This should be called
// after |MissiveClientTestObserver| has been initialized and device settings
// have been set.
//
// To use this utility, create an instance as a fixture in the test class and
// pass in |DevicePolicyCrosBrowserTest::device_state_| to the constructor. This
// class maintains a pointer to the device state, and therefore it must be
// destroyed before the device state is. Check out
// network/network_info_sampler_browsertest.cc for an example.
class MetricTestInitializationHelper
    : public policy::CloudPolicyCore::Observer {
 public:
  explicit MetricTestInitializationHelper(ash::DeviceStateMixin* device_state);
  ~MetricTestInitializationHelper() override;

  // Run |MetricReportingManager::DelayedInit| by advancing the mock clock.
  void SetUpDelayedInitialization();

 protected:
  // Called after the core is connected.
  void OnCoreConnected(policy::CloudPolicyCore* core) override;
  // Called after the refresh scheduler is started.
  void OnRefreshSchedulerStarted(policy::CloudPolicyCore* core) override;
  // Called before the core is disconnected.
  void OnCoreDisconnecting(policy::CloudPolicyCore* core) override;

 private:
  std::unique_ptr<base::ScopedMockTimeMessageLoopTaskRunner> mock_task_runner_;
  std::unique_ptr<base::RunLoop> run_loop_;
  ash::DeviceStateMixin* device_state_;
};
}  // namespace ash::reporting

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_METRIC_BROWSERTEST_UTILS_H_
