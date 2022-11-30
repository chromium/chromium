// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/web_contents/printing_tag.h"

#include <memory>

namespace task_manager {

std::unique_ptr<RendererTask> PrintingTag::CreateTask(
    WebContentsTaskProvider*) const {
  return std::make_unique<PrintingTask>(web_contents());
}

PrintingTag::PrintingTag(content::WebContents* web_contents)
    : WebContentsTag(web_contents) {
}

PrintingTag::~PrintingTag() {
}

}  // namespace task_manager
