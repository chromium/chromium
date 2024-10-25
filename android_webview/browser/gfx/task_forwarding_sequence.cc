// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/gfx/task_forwarding_sequence.h"

#include "android_webview/browser/gfx/task_queue_webview.h"
#include "base/notreached.h"

namespace android_webview {

TaskForwardingSequence::TaskForwardingSequence(TaskQueueWebView* task_queue)
    : task_queue_(task_queue) {
  task_queue_->EnsureSequenceInitialized();
}

TaskForwardingSequence::~TaskForwardingSequence() = default;

gpu::SequenceId TaskForwardingSequence::GetSequenceId() {
  return task_queue_->GetSequenceId();
}

bool TaskForwardingSequence::ShouldYield() {
  return false;
}

void TaskForwardingSequence::ScheduleTask(
    base::OnceClosure task,
    std::vector<gpu::SyncToken> sync_token_fences,
    const gpu::SyncToken& release,
    ReportingCallback report_callback) {
  task_queue_->ScheduleTask(std::move(task), std::move(sync_token_fences),
                            release, std::move(report_callback));
}

void TaskForwardingSequence::ScheduleOrRetainTask(
    base::OnceClosure task,
    std::vector<gpu::SyncToken> sync_token_fences,
    const gpu::SyncToken& release,
    ReportingCallback report_callback) {
  task_queue_->ScheduleOrRetainTask(std::move(task),
                                    std::move(sync_token_fences), release,
                                    std::move(report_callback));
}

// Should not be called because tasks aren't reposted to wait for sync tokens,
// or for yielding execution since ShouldYield() returns false.
void TaskForwardingSequence::ContinueTask(base::OnceClosure task) {
  NOTREACHED();
}

gpu::ScopedSyncPointClientState
TaskForwardingSequence::CreateSyncPointClientState(
    gpu::CommandBufferNamespace namespace_id,
    gpu::CommandBufferId command_buffer_id) {
  return task_queue_->CreateSyncPointClientState(namespace_id,
                                                 command_buffer_id);
}

}  // namespace android_webview
