// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_METRIC_BROWSERTEST_UTILS_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_METRIC_BROWSERTEST_UTILS_H_

#include "base/test/scoped_mock_time_message_loop_task_runner.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "content/public/test/browser_test.h"

namespace ash::reporting {

// The base class of metric browser tests.

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
// Check out network/network_info_sampler_browsertest.cc for an example.
class MetricBrowserTestBase : public policy::DevicePolicyCrosBrowserTest,
                              public policy::CloudPolicyCore::Observer {
 protected:
  MetricBrowserTestBase();
  ~MetricBrowserTestBase() override;

  // Called after the core is connected.
  void OnCoreConnected(policy::CloudPolicyCore* core) override;
  // Called after the refresh scheduler is started.
  void OnRefreshSchedulerStarted(policy::CloudPolicyCore* core) override;
  // Called before the core is disconnected.
  void OnCoreDisconnecting(policy::CloudPolicyCore* core) override;

  // Run |MetricReportingManager::DelayedInit| by advancing the mock clock.
  void SetUpDelayedInitialization();

 private:
  std::unique_ptr<base::ScopedMockTimeMessageLoopTaskRunner> mock_task_runner_;
  std::unique_ptr<base::RunLoop> run_loop_;
};
}  // namespace ash::reporting

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_METRIC_BROWSERTEST_UTILS_H_
