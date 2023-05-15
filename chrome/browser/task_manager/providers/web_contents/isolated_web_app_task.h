// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_ISOLATED_WEB_APP_TASK_H_
#define CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_ISOLATED_WEB_APP_TASK_H_

#include "chrome/browser/task_manager/providers/web_contents/renderer_task.h"

namespace task_manager {

// Defines a task manager representation for an Isolated Web App.
class IsolatedWebAppTask : public RendererTask {
 public:
  explicit IsolatedWebAppTask(content::WebContents* web_contents);
  IsolatedWebAppTask(const IsolatedWebAppTask&) = delete;
  IsolatedWebAppTask& operator=(const IsolatedWebAppTask&) = delete;
  ~IsolatedWebAppTask() override;

  // task_manager::RendererTask:
  void UpdateTitle() override;
  void UpdateFavicon() override;
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_ISOLATED_WEB_APP_TASK_H_
