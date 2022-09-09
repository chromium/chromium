// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_PRERENDER_TASK_H_
#define CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_PRERENDER_TASK_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/task_manager/providers/web_contents/renderer_task.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace task_manager {

class WebContentsTaskProvider;

// Defines a task manager entry for a Prerender2 rule.
class PrerenderTask : public RendererTask {
 public:
  explicit PrerenderTask(content::RenderFrameHost* render_frame_host,
                         WebContentsTaskProvider* task_provider);
  PrerenderTask(const PrerenderTask&) = delete;
  PrerenderTask& operator=(const PrerenderTask&) = delete;
  ~PrerenderTask() override;

  // task_manager::Task:
  const Task* GetParentTask() const override;
  // task_manager::RendererTask:
  void UpdateTitle() override;
  void UpdateFavicon() override {}

 private:
  std::u16string GetTitle() const;
  // |render_frame_host_| is guaranteed to outlive the task because the
  // prerender2 task is deleted at |WebContentsEntry::RenderFrameDeleted| and
  // |WebContentsEntry::RenderFrameHostStateChange|, and at both occasions the
  // RFH is still alive.
  const raw_ptr<content::RenderFrameHost> render_frame_host_;
  // The provider has the same lifespan as the task manager.
  const raw_ptr<WebContentsTaskProvider> task_provider_;
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_PRERENDER_TASK_H_
