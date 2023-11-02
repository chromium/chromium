// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_TOOL_TAG_H_
#define CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_TOOL_TAG_H_

#include "chrome/browser/task_manager/providers/web_contents/tool_task.h"
#include "chrome/browser/task_manager/providers/web_contents/web_contents_tag.h"

namespace task_manager {

// Defines a concrete UserData type for WebContents owned by extensions.
class ToolTag : public WebContentsTag {
 public:
  ~ToolTag() override;
  ToolTag(const ToolTag&) = delete;
  ToolTag& operator=(const ToolTag&) = delete;

  // task_manager::WebContentsTag:
  std::unique_ptr<RendererTask> CreateTask(
      WebContentsTaskProvider*) const override;

 private:
  friend class WebContentsTags;

  ToolTag(content::WebContents* web_contents, int tool_name);

  // The string ID of the name of this tool.
  const int tool_name_;
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_TOOL_TAG_H_
