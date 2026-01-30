// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_utils.h"

#include "build/build_config.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"
#endif

namespace contextual_tasks {

ContextualTasksUIInterface* GetWebUiInterface(
    content::WebContents* web_contents) {
#if !BUILDFLAG(IS_ANDROID)
  if (!web_contents || !web_contents->GetWebUI()) {
    return nullptr;
  }

  return web_contents->GetWebUI()->GetController()->GetAs<ContextualTasksUI>();
#else
  // TODO(crbug.com/478283549): Provide android implementation.
  return nullptr;
#endif
}

}  // namespace contextual_tasks
