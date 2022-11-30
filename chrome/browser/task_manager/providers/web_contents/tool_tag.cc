// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/web_contents/tool_tag.h"

#include <memory>

#include "chrome/browser/task_manager/providers/web_contents/tool_task.h"
#include "content/public/browser/web_contents.h"

namespace task_manager {

std::unique_ptr<RendererTask> ToolTag::CreateTask(
    WebContentsTaskProvider*) const {
  return std::make_unique<ToolTask>(web_contents(), tool_name_);
}

ToolTag::ToolTag(content::WebContents* web_contents, int tool_name)
    : WebContentsTag(web_contents), tool_name_(tool_name) {}

ToolTag::~ToolTag() = default;

}  // namespace task_manager
