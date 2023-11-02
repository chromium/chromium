// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/web_contents/portal_tag.h"

#include <memory>

namespace task_manager {

std::unique_ptr<RendererTask> PortalTag::CreateTask(
    WebContentsTaskProvider* task_provider) const {
  return std::make_unique<PortalTask>(web_contents(), task_provider);
}

PortalTag::PortalTag(content::WebContents* web_contents)
    : WebContentsTag(web_contents) {}

PortalTag::~PortalTag() = default;

}  // namespace task_manager
