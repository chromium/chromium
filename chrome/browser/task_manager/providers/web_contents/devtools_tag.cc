// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/web_contents/devtools_tag.h"

#include <memory>

namespace task_manager {

std::unique_ptr<RendererTask> DevToolsTag::CreateTask(
    WebContentsTaskProvider*) const {
  return std::make_unique<DevToolsTask>(web_contents());
}

DevToolsTag::DevToolsTag(content::WebContents* web_contents)
    : WebContentsTag(web_contents) {
}

DevToolsTag::~DevToolsTag() {
}

}  // namespace task_manager
