// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_PORTAL_TAG_H_
#define CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_PORTAL_TAG_H_

#include "chrome/browser/task_manager/providers/web_contents/portal_task.h"
#include "chrome/browser/task_manager/providers/web_contents/web_contents_tag.h"

namespace task_manager {

// Labels Portal WebContents so that they are appropriately described in the
// task manager.
class PortalTag : public WebContentsTag {
 public:
  PortalTag(const PortalTag&) = delete;
  PortalTag& operator=(const PortalTag&) = delete;
  ~PortalTag() override;

  // task_manager::WebContentsTag:
  std::unique_ptr<RendererTask> CreateTask(
      WebContentsTaskProvider* task_provider) const override;

 private:
  friend class WebContentsTags;

  explicit PortalTag(content::WebContents* web_contents);
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_PORTAL_TAG_H_
