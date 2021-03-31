// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_BACKGROUND_CONTENTS_TAG_H_
#define CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_BACKGROUND_CONTENTS_TAG_H_

#include "base/macros.h"
#include "chrome/browser/task_manager/providers/web_contents/background_contents_task.h"
#include "chrome/browser/task_manager/providers/web_contents/web_contents_tag.h"

class BackgroundContents;

namespace task_manager {

// Defines a concrete UserData type for WebContents owned by BackgroundContents
// service.
class BackgroundContentsTag : public WebContentsTag {
 public:
  ~BackgroundContentsTag() override;

  // task_manager::WebContentsTag:
  std::unique_ptr<RendererTask> CreateTask(
      WebContentsTaskProvider*) const override;

 private:
  friend class WebContentsTags;

  BackgroundContentsTag(content::WebContents* web_contents,
                        BackgroundContents* background_contents);

  // The owning BackgroundContents.
  BackgroundContents* background_contents_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundContentsTag);
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_BACKGROUND_CONTENTS_TAG_H_
