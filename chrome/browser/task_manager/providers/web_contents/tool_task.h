// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_TOOL_TASK_H_
#define CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_TOOL_TASK_H_

#include "chrome/browser/task_manager/providers/web_contents/renderer_task.h"

namespace task_manager {

// Defines a task manager representation for WebContents that are created for
// various UI tools.
class ToolTask : public RendererTask {
 public:
  ToolTask(content::WebContents* web_contents, int tool_name);
  ~ToolTask() override;
  ToolTask(const ToolTask&) = delete;
  ToolTask& operator=(const ToolTask&) = delete;

  // task_manager::RendererTask:
  void UpdateTitle() override;
  void UpdateFavicon() override;

 private:
  static gfx::ImageSkia* s_icon_;
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_TOOL_TASK_H_
