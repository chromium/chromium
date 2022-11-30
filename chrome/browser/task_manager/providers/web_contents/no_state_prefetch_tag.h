// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_NO_STATE_PREFETCH_TAG_H_
#define CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_NO_STATE_PREFETCH_TAG_H_

#include "chrome/browser/task_manager/providers/web_contents/no_state_prefetch_task.h"
#include "chrome/browser/task_manager/providers/web_contents/web_contents_tag.h"

namespace task_manager {

// Defines a concrete UserData type for WebContents owned by the
// NoStatePrefetchManager.
class NoStatePrefetchTag : public WebContentsTag {
 public:
  NoStatePrefetchTag(const NoStatePrefetchTag&) = delete;
  NoStatePrefetchTag& operator=(const NoStatePrefetchTag&) = delete;
  ~NoStatePrefetchTag() override;

  // task_manager::WebContentsTag:
  std::unique_ptr<RendererTask> CreateTask(
      WebContentsTaskProvider*) const override;

 private:
  friend class WebContentsTags;

  explicit NoStatePrefetchTag(content::WebContents* web_contents);
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_NO_STATE_PREFETCH_TAG_H_
