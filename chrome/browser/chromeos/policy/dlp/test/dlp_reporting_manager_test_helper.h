// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_TEST_DLP_REPORTING_MANAGER_TEST_HELPER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_TEST_DLP_REPORTING_MANAGER_TEST_HELPER_H_

#include "base/task/sequenced_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

class DlpPolicyEvent;

namespace policy {

class DlpReportingManager;

::testing::Matcher<const DlpPolicyEvent&> IsDlpPolicyEvent(
    const DlpPolicyEvent& event);

// Sets MockReportQueue for DlpReportingManager. Whenever AddRecord function of
// MockReportQueue is called (a DLP restriction is triggered) a new
// DlpPolicyEvent is pushed to |events|.
void SetReportQueueForReportingManager(
    policy::DlpReportingManager* manager,
    std::vector<DlpPolicyEvent>& events,
    scoped_refptr<base::SequencedTaskRunner> task_runner);

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_TEST_DLP_REPORTING_MANAGER_TEST_HELPER_H_
