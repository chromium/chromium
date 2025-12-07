// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_shutdown_monitor.h"

#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ash/borealis/borealis_context_manager.h"
#include "chrome/browser/ash/borealis/borealis_service.h"
#include "chrome/browser/ash/borealis/borealis_service_factory.h"

namespace {

// The default time period used when initiating delayed shutdowns.
constexpr base::TimeDelta kDefaultDelay = base::Seconds(60);

}  // namespace

namespace borealis {

BorealisShutdownMonitor::BorealisShutdownMonitor(Profile* profile)
    : profile_(profile), delay_(kDefaultDelay) {}

BorealisShutdownMonitor::~BorealisShutdownMonitor() = default;

void BorealisShutdownMonitor::ShutdownWithDelay() {
  // Reset() cancels the previous request if it was already there. Also,
  // Unretained() is safe because the callback is cancelled when
  // |in_progress_request_| is destroyed.
  in_progress_request_.Reset(
      base::BindOnce(&BorealisShutdownMonitor::OnShutdownTimerElapsed,
                     base::Unretained(this)));
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, in_progress_request_.callback(), delay_);
}

void BorealisShutdownMonitor::ShutdownNow() {
  BorealisServiceFactory::GetForProfile(profile_)
      ->ContextManager()
      .ShutDownBorealis();
}

void BorealisShutdownMonitor::CancelDelayedShutdown() {
  in_progress_request_.Cancel();
}

void BorealisShutdownMonitor::SetShutdownDelayForTesting(
    base::TimeDelta delay) {
  delay_ = delay;
}

void BorealisShutdownMonitor::OnShutdownTimerElapsed() {
  // TODO(b/198698779): Remove this log line when it is no longer needed.
  LOG(WARNING) << "Automatic shutdown triggered";
  ShutdownNow();
}

}  // namespace borealis
