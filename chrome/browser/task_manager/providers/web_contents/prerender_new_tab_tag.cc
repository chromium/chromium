// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/web_contents/prerender_new_tab_tag.h"

#include <memory>

namespace task_manager {

PrerenderNewTabTag::PrerenderNewTabTag(content::WebContents* web_contents)
    : WebContentsTag(web_contents) {}

PrerenderNewTabTag::~PrerenderNewTabTag() = default;

std::unique_ptr<RendererTask> PrerenderNewTabTag::CreateTask(
    WebContentsTaskProvider*) const {
  return std::make_unique<PrerenderNewTabTask>(web_contents());
}

}  // namespace task_manager
