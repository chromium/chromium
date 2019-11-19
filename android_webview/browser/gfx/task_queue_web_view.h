// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_GFX_TASK_QUEUE_WEB_VIEW_H_
#define ANDROID_WEBVIEW_BROWSER_GFX_TASK_QUEUE_WEB_VIEW_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/single_thread_task_runner.h"

namespace android_webview {

// This class is used to control when to access GL.
class ScopedAllowGL {
 public:
  ScopedAllowGL();
  ~ScopedAllowGL();

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedAllowGL);
};

// In WebView, there is a single task queue that runs all tasks instead of
// thread task runners. This is the class actually scheduling and running tasks
// for WebView. This is used by both CommandBuffer and SkiaDDL.
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

  // Called by DeferredGpuCommandService to schedule delayed tasks.
  // This should not be called when kVizForWebView is enabled.
  virtual void ScheduleIdleTask(base::OnceClosure task) = 0;

  // Called by both DeferredGpuCommandService and
  // SkiaOutputSurfaceDisplayContext to post task to client thread.
  virtual void ScheduleClientTask(base::OnceClosure task) = 0;

 protected:
  friend ScopedAllowGL;

  virtual ~TaskQueueWebView() = default;

  // Called by ScopedAllowGL.
  virtual void RunAllTasks() = 0;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_GFX_TASK_QUEUE_WEB_VIEW_H_
