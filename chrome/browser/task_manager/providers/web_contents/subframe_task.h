// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_SUBFRAME_TASK_H_
#define CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_SUBFRAME_TASK_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/task_manager/providers/web_contents/renderer_task.h"

namespace content {
class RenderFrameHost;
class SiteInstance;
class WebContents;
}  // namespace content

namespace task_manager {

// Defines a concrete renderer task that can represent processes hosting
// out-of-process iframes.
class SubframeTask : public RendererTask {
 public:
  SubframeTask(content::RenderFrameHost* render_frame_host,
               RendererTask* main_task);
  SubframeTask(const SubframeTask&) = delete;
  SubframeTask& operator=(const SubframeTask&) = delete;
  ~SubframeTask() override;

  // task_manager::RendererTask:
  void UpdateTitle() override;
  void UpdateFavicon() override;
  void Activate() override;

  // task_manager::Task:
  Task* GetParentTask() const override;

 private:
  std::u16string GetTitle();

  raw_ptr<content::SiteInstance, DanglingUntriaged> site_instance_;

  // The task for the main frame of this WebContents.
  raw_ptr<RendererTask, DanglingUntriaged> main_task_;
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_SUBFRAME_TASK_H_
