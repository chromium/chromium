// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/gfx/task_forwarding_sequence.h"

#include "android_webview/browser/gfx/task_queue_webview.h"
#include "base/functional/bind.h"
#include "base/synchronization/waitable_event.h"
#include "base/trace_event/trace_event.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/sync_point_manager.h"

namespace android_webview {

TaskForwardingSequence::TaskForwardingSequence(
    TaskQueueWebView* task_queue,
    gpu::SyncPointManager* sync_point_manager,
    gpu::Scheduler* scheduler)
    : gpu::SingleTaskSequence(),
      task_queue_(task_queue),
      sync_point_manager_(sync_point_manager),
      scheduler_(scheduler),
      sync_point_order_data_(sync_point_manager_->CreateSyncPointOrderData()) {}

TaskForwardingSequence::~TaskForwardingSequence() {
  sync_point_order_data_->Destroy();
}

// CommandBufferTaskExecutor::Sequence implementation.
gpu::SequenceId TaskForwardingSequence::GetSequenceId() {
  return sync_point_order_data_->sequence_id();
}

bool TaskForwardingSequence::ShouldYield() {
  return false;
}

void TaskForwardingSequence::ScheduleTask(
    base::OnceClosure task,
    std::vector<gpu::SyncToken> sync_token_fences,
    ReportingCallback report_callback) {
  uint32_t order_num = sync_point_order_data_->GenerateUnprocessedOrderNumber();

  // |sync_point_manager_| is global so it's safe to pass raw pointer.
  // sync_point_order_data_ is ThreadSafe.
  task_queue_->ScheduleTask(
      base::BindOnce(&TaskForwardingSequence::RunTask, std::move(task),
                     std::move(sync_token_fences), order_num,
                     sync_point_manager_, scheduler_, sync_point_order_data_),
      false /* out_of_order */);
}

void TaskForwardingSequence::ScheduleOrRetainTask(
    base::OnceClosure task,
    std::vector<gpu::SyncToken> sync_token_fences,
    ReportingCallback report_callback) {
  uint32_t order_num = sync_point_order_data_->GenerateUnprocessedOrderNumber();

  // |sync_point_manager_| is global so it's safe to pass raw pointer.
  // sync_point_order_data_ is ThreadSafe.
  task_queue_->ScheduleOrRetainTask(
      base::BindOnce(&TaskForwardingSequence::RunTask, std::move(task),
                     std::move(sync_token_fences), order_num,
                     sync_point_manager_, scheduler_, sync_point_order_data_));
}

// Should not be called because tasks aren't reposted to wait for sync tokens,
// or for yielding execution since ShouldYield() returns false.
void TaskForwardingSequence::ContinueTask(base::OnceClosure task) {
  NOTREACHED();
}

// Method to wrap scheduled task with the order number processing required for
// sync tokens.
void TaskForwardingSequence::RunTask(
    base::OnceClosure task,
    std::vector<gpu::SyncToken> sync_token_fences,
    uint32_t order_num,
    gpu::SyncPointManager* sync_point_manager,
    gpu::Scheduler* scheduler,
    scoped_refptr<gpu::SyncPointOrderData> sync_point_order_data) {
  // Block thread when waiting for sync token. This avoids blocking when we
  // encounter the wait command later.
  for (const auto& sync_token : gpu::ReduceSyncTokens(sync_token_fences)) {
    base::WaitableEvent completion;
    if (sync_point_manager->Wait(
            sync_token, sync_point_order_data->sequence_id(), order_num,
            base::BindOnce(&base::WaitableEvent::Signal,
                           base::Unretained(&completion)))) {
      gpu::SequenceId release_sequence_id =
          sync_point_manager->GetSyncTokenReleaseSequenceId(sync_token);
      TRACE_EVENT1("android_webview",
                   "TaskForwardingSequence::RunTask::WaitSyncToken",
                   "sequence_id", release_sequence_id.value());
      gpu::Scheduler::ScopedSetSequencePriority waiting(
          scheduler, release_sequence_id, gpu::SchedulingPriority::kHigh);
      completion.Wait();
    }
  }
  sync_point_order_data->BeginProcessingOrderNumber(order_num);
  std::move(task).Run();
  sync_point_order_data->FinishProcessingOrderNumber(order_num);
}

}  // namespace android_webview
