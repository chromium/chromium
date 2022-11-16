// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_BACK_FORWARD_CACHE_TASK_H_
#define CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_BACK_FORWARD_CACHE_TASK_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/task_manager/providers/web_contents/renderer_task.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace task_manager {

class WebContentsTaskProvider;

// Defines a concrete renderer task that can represent processes stored in the
// BackForwardCache. Tasks are added for each cached main frame, as well as for
// each separate SiteInstance for subframes within a cached page.
class BackForwardCacheTask : public RendererTask {
 public:
  BackForwardCacheTask(content::RenderFrameHost* render_frame_host,
                       RendererTask* parent_task,
                       WebContentsTaskProvider* task_provider);
  BackForwardCacheTask(const BackForwardCacheTask&) = delete;
  BackForwardCacheTask& operator=(const BackForwardCacheTask&) = delete;
  ~BackForwardCacheTask() override = default;

  // task_manager::Task:
  Task* GetParentTask() const override;

  // task_manager::RendererTask:
  void Activate() override;
  void UpdateTitle() override;
  void UpdateFavicon() override;

 private:
  // For a cached subframe task, |parent_task_| is a cached main frame task the
  // subframe task can be associated with. It is used to help group entries in
  // the task manager. As with active subframe tasks only one task manager entry
  // is created per site. Therefore a 1:1 mapping of main frame task to subframe
  // task is not guaranteed.
  // For cached main frame tasks |parent_task_| is nullptr.
  raw_ptr<RendererTask, DanglingUntriaged> parent_task_;
  // The provider has the same lifespan as the task manager.
  const raw_ptr<WebContentsTaskProvider, DanglingUntriaged> task_provider_;
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_BACK_FORWARD_CACHE_TASK_H_
