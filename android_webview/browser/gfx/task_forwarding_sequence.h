// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_GFX_TASK_FORWARDING_SEQUENCE_H_
#define ANDROID_WEBVIEW_BROWSER_GFX_TASK_FORWARDING_SEQUENCE_H_

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "gpu/command_buffer/service/single_task_sequence.h"

namespace gpu {
class Scheduler;
class SyncPointManager;
}

namespace android_webview {
class TaskQueueWebView;

// TaskForwardingSequence provides a SingleTaskSequence implementation that
// satisfies WebView's threading requirements. It encapsulates a
// SyncPointOrderData, and posts tasks to the WebView's global task queue:
// TaskQueueWebView.
//
// Lifetime: WebView
class TaskForwardingSequence : public gpu::SingleTaskSequence {
 public:
  TaskForwardingSequence(TaskQueueWebView* task_queue,
                         gpu::SyncPointManager* sync_point_manager,
                         gpu::Scheduler* scheduler);

  TaskForwardingSequence(const TaskForwardingSequence&) = delete;
  TaskForwardingSequence& operator=(const TaskForwardingSequence&) = delete;

  ~TaskForwardingSequence() override;

  // SingleTaskSequence implementation.
  gpu::SequenceId GetSequenceId() override;

  // There is only one task queue. ShouldYield always return false.
  bool ShouldYield() override;

  void ScheduleTask(
      base::OnceClosure task,
      std::vector<gpu::SyncToken> sync_token_fences,
      ReportingCallback report_callback = ReportingCallback()) override;
  void ScheduleOrRetainTask(
      base::OnceClosure task,
      std::vector<gpu::SyncToken> sync_token_fences,
      ReportingCallback report_callback = ReportingCallback()) override;

  // Should not be called because tasks aren't reposted to wait for sync tokens,
  // or for yielding execution since ShouldYield() returns false.
  void ContinueTask(base::OnceClosure task) override;

 private:
  // Method to wrap scheduled task with the order number processing required for
  // sync tokens.
  static void RunTask(
      base::OnceClosure task,
      std::vector<gpu::SyncToken> sync_token_fences,
      uint32_t order_num,
      gpu::SyncPointManager* sync_point_manager,
      gpu::Scheduler* scheduler,
      scoped_refptr<gpu::SyncPointOrderData> sync_point_order_data);

  // Raw pointer refer to the global instance.
  const raw_ptr<TaskQueueWebView> task_queue_;
  const raw_ptr<gpu::SyncPointManager> sync_point_manager_;
  const raw_ptr<gpu::Scheduler> scheduler_;
  scoped_refptr<gpu::SyncPointOrderData> sync_point_order_data_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_GFX_TASK_FORWARDING_SEQUENCE_H_
