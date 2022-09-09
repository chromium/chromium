// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/delayed_callback_runner.h"

#include "base/location.h"
#include "content/public/browser/browser_thread.h"

namespace safe_browsing {

DelayedCallbackRunner::DelayedCallbackRunner(
    base::TimeDelta delay,
    const scoped_refptr<base::TaskRunner>& task_runner)
    : task_runner_(task_runner),
      has_work_(false),
      timer_(FROM_HERE, delay, this, &DelayedCallbackRunner::OnTimer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

DelayedCallbackRunner::~DelayedCallbackRunner() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void DelayedCallbackRunner::RegisterCallback(base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  callbacks_.push(std::move(callback));
}

void DelayedCallbackRunner::Start() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Nothing to do if the runner is already running or nothing has been added.
  if (has_work_ || callbacks_.empty())
    return;

  // Prime the system with the first callback.
  has_work_ = true;

  // Point the starter pistol in the air and pull the trigger.
  timer_.Reset();
}

void DelayedCallbackRunner::OnTimer() {
  // Run the next callback on the task runner.
  task_runner_->PostTask(FROM_HERE, std::move(callbacks_.front()));
  callbacks_.pop();

  // Remove this callback and get ready for the next if there is one.
  has_work_ = !callbacks_.empty();
  if (has_work_)
    timer_.Reset();
}

}  // namespace safe_browsing
