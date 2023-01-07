// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_DELAYED_CALLBACK_RUNNER_H_
#define CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_DELAYED_CALLBACK_RUNNER_H_

#include "base/containers/queue.h"
#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/task/task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace safe_browsing {

// Runs callbacks on a given task runner, waiting a certain amount of time
// between each. The delay also applies to running the first callback (i.e.,
// the first callback will be run some time after Start() is invoked). Callbacks
// are deleted after they are run. Start() is idempotent: calling it while the
// runner is doing its job has no effect.
// Lives on the UI thread.
class DelayedCallbackRunner {
 public:
  // Constructs an instance that runs tasks on |callback_runner|, waiting for
  // |delay| time to pass before and between each callback.
  DelayedCallbackRunner(base::TimeDelta delay,
                        const scoped_refptr<base::TaskRunner>& task_runner);

  DelayedCallbackRunner(const DelayedCallbackRunner&) = delete;
  DelayedCallbackRunner& operator=(const DelayedCallbackRunner&) = delete;

  ~DelayedCallbackRunner();

  // Registers |callback| with the runner. A copy of |callback| is held until it
  // is run.
  void RegisterCallback(base::OnceClosure callback);

  // Starts running the callbacks after the delay.
  void Start();

 private:
  using CallbackList = base::queue<base::OnceClosure>;

  // A callback invoked by the timer to run the next callback. The timer is
  // restarted to process the next callback if there is one.
  void OnTimer();

  // The runner on which callbacks are to be run.
  scoped_refptr<base::TaskRunner> task_runner_;

  // The list of callbacks to run. Callbacks are removed when run.
  CallbackList callbacks_;

  // Whethere there is work to be done from |callbacks_|.
  bool has_work_;

  // A timer upon the firing of which the next callback will be run.
  base::DelayTimer timer_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_DELAYED_CALLBACK_RUNNER_H_
