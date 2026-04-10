// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_PRERENDER_NEW_TAB_TASK_H_
#define CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_PRERENDER_NEW_TAB_TASK_H_

#include "chrome/browser/task_manager/providers/web_contents/renderer_task.h"

namespace task_manager {

// Defines a task manager representation of WebContents created by
// PrerenderNewTabHandle for prerendering in a new tab (target=_blank).
class PrerenderNewTabTask : public RendererTask {
 public:
  explicit PrerenderNewTabTask(content::WebContents* web_contents);
  PrerenderNewTabTask(const PrerenderNewTabTask&) = delete;
  PrerenderNewTabTask& operator=(const PrerenderNewTabTask&) = delete;
  ~PrerenderNewTabTask() override;

  // task_manager::RendererTask:
  void UpdateTitle() override;
  void UpdateFavicon() override;
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_PRERENDER_NEW_TAB_TASK_H_
