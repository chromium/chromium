// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_PORTAL_TASK_H_
#define CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_PORTAL_TASK_H_

#include "chrome/browser/task_manager/providers/web_contents/renderer_task.h"

namespace task_manager {

class WebContentsTaskProvider;

// Represents the fact that a process might be responsible for the task of
// rendering a portal WebContents.
class PortalTask : public RendererTask {
 public:
  explicit PortalTask(content::WebContents* web_contents,
                      WebContentsTaskProvider* task_provider);
  PortalTask(const PortalTask&) = delete;
  PortalTask& operator=(const PortalTask&) = delete;
  ~PortalTask() override;

  // task_manager::RendererTask:
  void UpdateTitle() override;
  void UpdateFavicon() override;
  void Activate() override;

  // task_manager::Task:
  const Task* GetParentTask() const override;

 private:
  WebContentsTaskProvider* task_provider_;
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_PORTAL_TASK_H_
