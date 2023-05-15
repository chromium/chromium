// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_FENCED_FRAME_TASK_H_
#define CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_FENCED_FRAME_TASK_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/task_manager/providers/web_contents/renderer_task.h"

namespace content {
class RenderFrameHost;
class SiteInstance;
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
  const task_manager::Task* GetParentTask() const override;
  // task_manager::RendererTask:
  void UpdateTitle() override;
  void UpdateFavicon() override {}

 private:
  std::u16string GetTitle() const;

  // The site instance is owned by the RFH and the RFH always outlives the task.
  // A FencedFrame task is deleted when:
  // 1, a navigation happens to the FencedFrame (FF), so
  //    |WebContentsEntry::RenderFrameHostChanged| is fired. Clearly the FF's
  //    RFH is alive (thus the site instance).
  // 2, when the fenced frame is destroyed (by either terminating the embedder
  //    or FF's process). The |RenderFrameDeleted| will be triggered to delete
  //    the task. At that point the RFH is still alive.
  const raw_ptr<content::SiteInstance> site_instance_;
  // Allows us to focus on the embedder's tab.
  const raw_ptr<RendererTask, FlakyDanglingUntriaged> embedder_task_;
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_FENCED_FRAME_TASK_H_
