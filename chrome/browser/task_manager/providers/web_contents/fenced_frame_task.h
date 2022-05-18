// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_FENCED_FRAME_TASK_H_
#define CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_FENCED_FRAME_TASK_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/task_manager/providers/web_contents/renderer_task.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace task_manager {

// Defines a task manager entry for a FencedFrame.
class FencedFrameTask : public RendererTask {
 public:
  FencedFrameTask(content::RenderFrameHost* render_frame_host,
                  RendererTask* embedder_task);
  FencedFrameTask(const FencedFrameTask&) = delete;
  FencedFrameTask& operator=(const FencedFrameTask&) = delete;
  ~FencedFrameTask() override = default;

  // task_manager::Task:
  void Activate() override;
  // task_manager::RendererTask:
  void UpdateTitle() override;
  void UpdateFavicon() override {}

 private:
  std::u16string GetTitle() const;

  // A FencedFrame task is deleted when:
  // 1, a navigation happens to the fenced frame, so
  //    |WebContentsEntry::RenderFrameHostChanged| is fired. Clearly in this
  //    case both the embedder frame (hence the task) as well as the FF's RFH
  //    are always alive.
  // 2, when the user closes the tab / terminates the embedder process from task
  //    manager/ terminates the FF's process from task manager. In the three
  //    cases the task will be deleted from
  //    |WebContentsEntry::RenderFrameDeleted|. When the user closes the tab or
  //    terminates the embedder process, the tasks will be deleted in a
  //    bottom-up fashion, from FF to embedder FF; when the user terminates the
  //    process of the FF, FF's frame is deleted. So in the three cases the RFH
  //    and embedder task are also guaranteed to outlive the FF task.
  raw_ptr<content::RenderFrameHost> render_frame_host_;
  // Allows us to focus on the embedder's tab.
  raw_ptr<RendererTask> embedder_task_;
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_FENCED_FRAME_TASK_H_
