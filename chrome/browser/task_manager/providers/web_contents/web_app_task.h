// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_WEB_APP_TASK_H_
#define CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_WEB_APP_TASK_H_

#include "chrome/browser/task_manager/providers/web_contents/renderer_task.h"

namespace task_manager {

// Defines a task manager representation for an Isolated Web App.
class WebAppTask : public RendererTask {
 public:
  explicit WebAppTask(content::WebContents* web_contents);
  WebAppTask(const WebAppTask&) = delete;
  WebAppTask& operator=(const WebAppTask&) = delete;
  ~WebAppTask() override;

  // task_manager::RendererTask:
  void UpdateTitle() override;
  void UpdateFavicon() override;

 private:
  std::u16string GetPrefixedTitle(content::WebContents* WebContents) const;
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_WEB_APP_TASK_H_
