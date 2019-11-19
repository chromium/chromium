// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_REPORTER_RUNNER_WIN_H_
#define CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_REPORTER_RUNNER_WIN_H_

#include <limits.h>

#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/sw_reporter_invocation_win.h"

namespace base {
class TaskRunner;
}

namespace safe_browsing {

class ChromeCleanerController;

namespace internal {

// A delegate used by tests to implement test doubles (e.g., stubs, fakes, or
// mocks).
//
// TODO(crbug.com/776538): Replace this with a proper delegate that defines the
// default behaviour to be overridden (instead of defined) by tests.
class SwReporterTestingDelegate {
 public:
  virtual ~SwReporterTestingDelegate() = default;

  // Invoked by tests in place of base::LaunchProcess.
  virtual base::Process LaunchReporterProcess(
      const SwReporterInvocation& invocation,
      const base::LaunchOptions& options) = 0;

  // Invoked by tests in place of base::Process::WaitForExit.
  virtual int WaitForReporterExit(
      const base::Process& reporter_process) const = 0;

  // Invoked by tests to override the current time.
  // See Now() in reporter_runner_win.cc.
  virtual base::Time Now() const = 0;

  // A task runner used to spawn the reporter process (which blocks).
  // See ReporterRunner::ScheduleNextInvocation().
  virtual base::TaskRunner* BlockingTaskRunner() const = 0;

  // Invoked by tests to return a mock to the cleaner controller.
  virtual ChromeCleanerController* GetCleanerController() = 0;

  // Invoked by tests in place of the actual creation of the dialog controller.
  virtual void CreateChromeCleanerDialogController() = 0;
};

// Set a delegate for testing. The implementation will not take ownership of
// |delegate| - it must remain valid until this function is called again to
// reset the delegate. If |delegate| is nullptr, any previous delegate is
// cleared.
void SetSwReporterTestingDelegate(SwReporterTestingDelegate* delegate);

// Returns true if the reporter is expected to terminate when the browser does
// on this version of Windows. Exposed for testing.
bool ReporterTerminatesOnBrowserExit();

// Launches a reporter process based on the command-line in |invocation| and
// returns its exit code. Exposed for testing.
int LaunchAndWaitForExit(const SwReporterInvocation& invocation);

}  // namespace internal

// A special exit code identifying a failure to run the reporter.
const int kReporterNotLaunchedExitCode = INT_MAX;

// The number of days to wait before triggering another reporter run.
const int kDaysBetweenSuccessfulSwReporterRuns = 7;
// The number of days to wait before sending out reporter logs.
const int kDaysBetweenReporterLogsSent = 7;

bool IsUserInitiated(SwReporterInvocationType invocation_type);

// Tries to run the given invocations. If this runs successfully, than any
// calls made in the next |kDaysBetweenSuccessfulSwReporterRuns| days will be
// ignored.
//
// Each "run" of the sw_reporter component may aggregate the results of several
// executions of the tool with different command lines. |invocations| is the
// queue of SwReporters to execute as a single "run". When a new try is
// scheduled the entire queue is executed.
void MaybeStartSwReporter(SwReporterInvocationType invocation_type,
                          SwReporterInvocationSequence&& invocations);

// Returns true if the sw_reporter is allowed to run due to enterprise policies.
bool SwReporterIsAllowedByPolicy();

// Returns true if the sw_reported is allowed to report back results due to
// enterprise policies.
bool SwReporterReportingIsAllowedByPolicy(Profile* profile);

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_REPORTER_RUNNER_WIN_H_
