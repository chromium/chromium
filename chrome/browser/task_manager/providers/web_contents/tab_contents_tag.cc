// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/web_contents/tab_contents_tag.h"

#include <memory>

namespace task_manager {

std::unique_ptr<RendererTask> TabContentsTag::CreateTask(
    WebContentsTaskProvider*) const {
  return std::make_unique<TabContentsTask>(web_contents());
}

TabContentsTag::TabContentsTag(content::WebContents* web_contents)
    : WebContentsTag(web_contents) {
}

TabContentsTag::~TabContentsTag() = default;

}  // namespace task_manager
