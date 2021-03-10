// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_shutdown_monitor.h"

#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/ash/borealis/borealis_context_manager.h"
#include "chrome/browser/ash/borealis/borealis_service.h"

namespace {

// The default time period used when initiating delayed shutdowns.
constexpr base::TimeDelta kDefaultDelay = base::TimeDelta::FromSeconds(60);

}  // namespace

namespace borealis {

BorealisShutdownMonitor::BorealisShutdownMonitor(Profile* profile)
    : profile_(profile), delay_(kDefaultDelay) {}

BorealisShutdownMonitor::~BorealisShutdownMonitor() = default;

void BorealisShutdownMonitor::ShutdownWithDelay() {
  // Reset() cancels the previous request if it was already there. Also,
  // Unretained() is safe because the callback is cancelled when
  // |in_progress_request_| is destroyed.
  in_progress_request_.Reset(base::BindOnce(
      &BorealisShutdownMonitor::ShutdownNow, base::Unretained(this)));
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, in_progress_request_.callback(), delay_);
}

void BorealisShutdownMonitor::ShutdownNow() {
  BorealisService::GetForProfile(profile_)->ContextManager().ShutDownBorealis();
}

void BorealisShutdownMonitor::CancelDelayedShutdown() {
  in_progress_request_.Cancel();
}

void BorealisShutdownMonitor::SetShutdownDelayForTesting(
    base::TimeDelta delay) {
  delay_ = delay;
}

}  // namespace borealis
