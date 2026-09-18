// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UI_POST_REARCHITECTURE_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UI_POST_REARCHITECTURE_H_

#include "chrome/browser/contextual_tasks/contextual_tasks_ui_base.h"
#include "content/public/browser/web_ui_controller.h"

namespace content {
class WebUI;
}  // namespace content

namespace contextual_tasks {

// The WebUI controller used after the Contextual Tasks rearchitecture.
// This class serves the toolbar WebUI in the post-rearchitecture world and
// remains lightweight, with shared logic living in ContextualTasksUIBase.
class ContextualTasksUIPostRearchitecture : public ContextualTasksUIBase {
 public:
  explicit ContextualTasksUIPostRearchitecture(content::WebUI* web_ui);
  ContextualTasksUIPostRearchitecture(
      const ContextualTasksUIPostRearchitecture&) = delete;
  ContextualTasksUIPostRearchitecture& operator=(
      const ContextualTasksUIPostRearchitecture&) = delete;
  ~ContextualTasksUIPostRearchitecture() override;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UI_POST_REARCHITECTURE_H_
