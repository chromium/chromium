// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_NO_STATE_PREFETCH_TASK_H_
#define CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_NO_STATE_PREFETCH_TASK_H_

#include "chrome/browser/task_manager/providers/web_contents/renderer_task.h"

namespace task_manager {

// Defines a task manager representation of WebContents owned by the
// NoStatePrefetchManager.
class NoStatePrefetchTask : public RendererTask {
 public:
  explicit NoStatePrefetchTask(content::WebContents* web_contents);
  NoStatePrefetchTask(const NoStatePrefetchTask&) = delete;
  NoStatePrefetchTask& operator=(const NoStatePrefetchTask&) = delete;
  ~NoStatePrefetchTask() override;

  // task_manager::RendererTask:
  void UpdateTitle() override;
  void UpdateFavicon() override;
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_NO_STATE_PREFETCH_TASK_H_
