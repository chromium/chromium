// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_PRERENDER_NEW_TAB_TAG_H_
#define CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_PRERENDER_NEW_TAB_TAG_H_

#include "chrome/browser/task_manager/providers/web_contents/prerender_new_tab_task.h"
#include "chrome/browser/task_manager/providers/web_contents/web_contents_tag.h"

namespace task_manager {

// Defines a concrete UserData type for WebContents created by
// PrerenderNewTabHandle for prerendering in a new tab.
class PrerenderNewTabTag : public WebContentsTag {
 public:
  PrerenderNewTabTag(const PrerenderNewTabTag&) = delete;
  PrerenderNewTabTag& operator=(const PrerenderNewTabTag&) = delete;
  ~PrerenderNewTabTag() override;

  // task_manager::WebContentsTag:
  std::unique_ptr<RendererTask> CreateTask(
      WebContentsTaskProvider*) const override;

 private:
  friend class WebContentsTags;

  explicit PrerenderNewTabTag(content::WebContents* web_contents);
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_PRERENDER_NEW_TAB_TAG_H_
