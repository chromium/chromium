// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_GFX_TASK_FORWARDING_SEQUENCE_H_
#define ANDROID_WEBVIEW_BROWSER_GFX_TASK_FORWARDING_SEQUENCE_H_

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/memory/weak_ptr.h"
#include "gpu/ipc/single_task_sequence.h"

namespace gpu {
class SyncPointManager;
}

namespace android_webview {
class TaskQueueWebView;

// TaskForwardingsequence provides a SingleTaskSequence implementation that
// satisfies WebView's threading requirements. It encapsulates a
// SyncPointOrderData, and posts tasks to the WebView's global task queue:
// TaskQueueWebView.
class TaskForwardingSequence : public gpu::SingleTaskSequence {
 public:
  explicit TaskForwardingSequence(TaskQueueWebView* task_queue,
                                  gpu::SyncPointManager* sync_point_manager);
  ~TaskForwardingSequence() override;

  // SingleTaskSequence implementation.
  gpu::SequenceId GetSequenceId() override;

  // There is only one task queue. ShouldYield always return false.
  bool ShouldYield() override;

  void ScheduleTask(base::OnceClosure task,
                    std::vector<gpu::SyncToken> sync_token_fences) override;
  void ScheduleOrRetainTask(
      base::OnceClosure task,
      std::vector<gpu::SyncToken> sync_token_fences) override;

  // Should not be called because tasks aren't reposted to wait for sync tokens,
  // or for yielding execution since ShouldYield() returns false.
  void ContinueTask(base::OnceClosure task) override;

 private:
  // Method to wrap scheduled task with the order number processing required for
  // sync tokens.
  void RunTask(base::OnceClosure task,
               std::vector<gpu::SyncToken> sync_token_fences,
               uint32_t order_num);

  // Raw pointer refer to the global instance.
  TaskQueueWebView* const task_queue_;
  gpu::SyncPointManager* const sync_point_manager_;
  scoped_refptr<gpu::SyncPointOrderData> sync_point_order_data_;
  base::WeakPtrFactory<TaskForwardingSequence> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(TaskForwardingSequence);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_GFX_TASK_FORWARDING_SEQUENCE_H_
