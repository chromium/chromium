// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_GFX_TASK_QUEUE_WEBVIEW_H_
#define ANDROID_WEBVIEW_BROWSER_GFX_TASK_QUEUE_WEBVIEW_H_

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"

namespace android_webview {

// In WebView, there is a single task queue that runs all tasks instead of
// thread task runners. This is the class actually scheduling and running tasks
// for WebView. This is used by both CommandBuffer and SkiaDDL.
//
// Lifetime: Singleton
class TaskQueueWebView {
 public:
  // Static method that makes sure this is only one copy of this class.
  static TaskQueueWebView* GetInstance();

  // Methods only used when kVizForWebView is enabled, ie client is the viz
  // thread.
  virtual void InitializeVizThread(
      const scoped_refptr<base::SingleThreadTaskRunner>& viz_task_runner) = 0;
  // The calling OnceClosure unblocks the render thread, and disallows further
  // calls to ScheduleTask.
  using VizTask = base::OnceCallback<void(base::OnceClosure)>;
  virtual void ScheduleOnVizAndBlock(VizTask viz_task) = 0;

  // Called by TaskForwardingSequence. |out_of_order| indicates if task should
  // be run ahead of already enqueued tasks.
  virtual void ScheduleTask(base::OnceClosure task, bool out_of_order) = 0;

  // Called by TaskForwardingSequence.
  virtual void ScheduleOrRetainTask(base::OnceClosure task) = 0;

  // Called to schedule delayed tasks.
  virtual void ScheduleIdleTask(base::OnceClosure task) = 0;

  // Used to post task to client thread.
  virtual scoped_refptr<base::SingleThreadTaskRunner> GetClientTaskRunner() = 0;

  // Uniti tests can switch render thread.
  virtual void ResetRenderThreadForTesting() = 0;

 protected:
  virtual ~TaskQueueWebView() = default;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_GFX_TASK_QUEUE_WEBVIEW_H_
