// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_BACKGROUND_CONTENTS_TASK_H_
#define CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_BACKGROUND_CONTENTS_TASK_H_

#include "base/macros.h"
#include "chrome/browser/background/background_contents.h"
#include "chrome/browser/task_manager/providers/web_contents/renderer_task.h"

namespace task_manager {

// Defines a RendererTask that represents background |WebContents|.
class BackgroundContentsTask : public RendererTask {
 public:
  BackgroundContentsTask(const std::u16string& title,
                         BackgroundContents* background_contents);
  ~BackgroundContentsTask() override;

  // task_manager::RendererTask:
  void UpdateTitle() override;
  void UpdateFavicon() override;

 private:
  static gfx::ImageSkia* s_icon_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundContentsTask);
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_BACKGROUND_CONTENTS_TASK_H_
