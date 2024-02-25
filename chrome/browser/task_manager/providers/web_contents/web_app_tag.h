// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_WEB_APP_TAG_H_
#define CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_WEB_APP_TAG_H_

#include "chrome/browser/task_manager/providers/web_contents/web_contents_tag.h"
#include "components/webapps/common/web_app_id.h"

namespace task_manager {

// Defines a concrete UserData type for WebContents owned by Web App.
class WebAppTag : public WebContentsTag {
 public:
  WebAppTag(const WebAppTag&) = delete;
  WebAppTag& operator=(const WebAppTag&) = delete;
  ~WebAppTag() override;

  // task_manager::WebContentsTag:
  std::unique_ptr<RendererTask> CreateTask(
      WebContentsTaskProvider*) const override;

 private:
  friend class WebContentsTags;

  WebAppTag(content::WebContents* web_contents,
            const webapps::AppId& app_id,
            const bool is_isolated_web_app);

  const webapps::AppId app_id_;
  const bool is_isolated_web_app_;
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_WEB_APP_TAG_H_
