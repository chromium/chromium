// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/web_contents/web_app_tag.h"

#include <memory>

#include "chrome/browser/task_manager/providers/web_contents/isolated_web_app_task.h"
#include "chrome/browser/task_manager/providers/web_contents/tab_contents_task.h"
#include "chrome/browser/task_manager/providers/web_contents/web_app_task.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/web_contents.h"

namespace task_manager {

std::unique_ptr<RendererTask> WebAppTag::CreateTask(
    WebContentsTaskProvider*) const {
  if (is_isolated_web_app_) {
    return std::make_unique<IsolatedWebAppTask>(web_contents());
  }

  return std::make_unique<WebAppTask>(web_contents());
}

WebAppTag::WebAppTag(content::WebContents* web_contents,
                     const webapps::AppId& app_id,
                     const bool is_isolated_web_app)
    : WebContentsTag(web_contents),
      app_id_(app_id),
      is_isolated_web_app_(is_isolated_web_app) {}

WebAppTag::~WebAppTag() = default;

}  // namespace task_manager
