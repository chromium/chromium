// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/web_contents/no_state_prefetch_tag.h"

#include <memory>

namespace task_manager {

NoStatePrefetchTag::NoStatePrefetchTag(content::WebContents* web_contents)
    : WebContentsTag(web_contents) {}

NoStatePrefetchTag::~NoStatePrefetchTag() = default;

std::unique_ptr<RendererTask> NoStatePrefetchTag::CreateTask(
    WebContentsTaskProvider*) const {
  return std::make_unique<NoStatePrefetchTask>(web_contents());
}

}  // namespace task_manager
