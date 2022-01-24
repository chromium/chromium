// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/util/critical_policy_section_metrics_win.h"

#include <userenv.h>
#include <windows.h>

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"

namespace chrome {
namespace enterprise_util {

namespace {

void DoCriticalPolicySectionMeasurement() {
  static constexpr struct {
    BOOL machine;
    const char* success_delay;
    const char* failure_delay;
  } kScopes[] = {
      {
          FALSE,
          "Enterprise.EnterCriticalPolicySectionDelay.User.Succeeded",
          "Enterprise.EnterCriticalPolicySectionDelay.User.Failed",
      },
      {
          TRUE,
          "Enterprise.EnterCriticalPolicySectionDelay.Machine.Succeeded",
          "Enterprise.EnterCriticalPolicySectionDelay.Machine.Failed",
      },
  };
  base::TimeDelta total_ticks;
  // MSDN suggests that processes needing to access both scopes acquire user
  // then machine to avoid deadlocks. Experimentation shows that this is not
  // enforced by the implementation, so evaluate the two independently.
  for (const auto& scope : kScopes) {
    const auto start_ticks = base::TimeTicks::Now();
    auto* section = ::EnterCriticalPolicySection(scope.machine);
    const auto stop_ticks = base::TimeTicks::Now();
    if (section) {
      ::LeaveCriticalPolicySection(section);
    } else {
      const auto error_code = ::GetLastError();
      base::UmaHistogramSparse("Enterprise.EnterCriticalPolicySectionError",
                               error_code);
    }
    // MSDN claims that any one process may hold the lock for at most 10
    // minutes. While that could mean that the browser may need to wait up to
    // N*10 minutes if it were behind N other slow processes in lock
    // acquisition, 10 is sufficient from an analysis standpoint -- anything in
    // that range is far too long for the user to be prevented from interacting
    // with the browser.
    const auto delta = stop_ticks - start_ticks;
    base::UmaHistogramCustomTimes(
        section ? scope.success_delay : scope.failure_delay, delta,
        base::Milliseconds(1), base::Minutes(10), 50);
    total_ticks += delta;
  }
  base::UmaHistogramCustomTimes(
      "Enterprise.EnterCriticalPolicySectionDelay.Total", total_ticks,
      base::Milliseconds(1), base::Minutes(20), 50);
}

}  // namespace

void MeasureAndReportCriticalPolicySectionAcquisition() {
  // The goal here is to perform the measurement in a task in the thread pool
  // running at USER_BLOCKING priority. This should be reasonably close to the
  // priority of normal browser startup (where policy retrieval happens). To
  // avoid interfering with any other work the browser may be doing, this task
  // is posted from a task in the thread pool running at BEST_EFFORT priority.
  // This should cause it to wait until after startup is complete and things
  // settle down.
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce([]() {
        base::ThreadPool::PostTask(
            FROM_HERE,
            {base::TaskPriority::USER_BLOCKING,
             base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN,
             base::MayBlock()},
            base::BindOnce(&DoCriticalPolicySectionMeasurement));
      }));
}

}  // namespace enterprise_util
}  // namespace chrome
