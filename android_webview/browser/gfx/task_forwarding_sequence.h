// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_GFX_TASK_FORWARDING_SEQUENCE_H_
#define ANDROID_WEBVIEW_BROWSER_GFX_TASK_FORWARDING_SEQUENCE_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "gpu/command_buffer/service/single_task_sequence.h"

namespace android_webview {
class TaskQueueWebView;

// TaskForwardingSequence is a simple wrapper that forwards all the calls to
// the WebView's global task queue TaskQueueWebView.
//
// TaskQueueWebView is a singleton. This wrapper allows users to hold
// std::unique_ptr<SingleTaskSequence>.
//
// Lifetime: WebView
class TaskForwardingSequence : public gpu::SingleTaskSequence {
 public:
  explicit TaskForwardingSequence(TaskQueueWebView* task_queue);

  TaskForwardingSequence(const TaskForwardingSequence&) = delete;
  TaskForwardingSequence& operator=(const TaskForwardingSequence&) = delete;

  ~TaskForwardingSequence() override;

  // gpu::SingleTaskSequence implementation.
  gpu::SequenceId GetSequenceId() override;

  // There is only one task queue. ShouldYield always return false.
  bool ShouldYield() override;

  void ScheduleTask(
      base::OnceClosure task,
      std::vector<gpu::SyncToken> sync_token_fences,
      const gpu::SyncToken& release,
      ReportingCallback report_callback = ReportingCallback()) override;
  void ScheduleOrRetainTask(
      base::OnceClosure task,
      std::vector<gpu::SyncToken> sync_token_fences,
      const gpu::SyncToken& release,
      ReportingCallback report_callback = ReportingCallback()) override;

  // Should not be called because tasks aren't reposted to wait for sync tokens,
  // or for yielding execution since ShouldYield() returns false.
  void ContinueTask(base::OnceClosure task) override;

  [[nodiscard]] gpu::ScopedSyncPointClientState CreateSyncPointClientState(
      gpu::CommandBufferNamespace namespace_id,
      gpu::CommandBufferId command_buffer_id) override;

 private:
  // Raw pointer refer to the global instance.
  const raw_ptr<TaskQueueWebView> task_queue_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_GFX_TASK_FORWARDING_SEQUENCE_H_
