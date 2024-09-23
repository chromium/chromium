// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_BACKGROUND_CONTENTS_TASK_H_
#define CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_BACKGROUND_CONTENTS_TASK_H_

#include "chrome/browser/task_manager/providers/web_contents/renderer_task.h"

class BackgroundContents;

namespace task_manager {

// Defines a RendererTask that represents background |WebContents|.
class BackgroundContentsTask : public RendererTask {
 public:
  BackgroundContentsTask(const std::u16string& title,
                         BackgroundContents* background_contents);
  BackgroundContentsTask(const BackgroundContentsTask&) = delete;
  BackgroundContentsTask& operator=(const BackgroundContentsTask&) = delete;
  ~BackgroundContentsTask() override;

  // task_manager::RendererTask:
  void UpdateTitle() override;
  void UpdateFavicon() override;

 private:
  static gfx::ImageSkia* s_icon_;
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_BACKGROUND_CONTENTS_TASK_H_
