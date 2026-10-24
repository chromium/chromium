// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_tab_restore_data.h"

#include <utility>

#include "components/contextual_tasks/public/features.h"

namespace contextual_tasks {

ContextualTasksTabRestoreData::ContextualTasksTabRestoreData(
    content::WebContents* web_contents,
    base::Uuid task_id,
    GURL thread_url)
    : content::WebContentsUserData<ContextualTasksTabRestoreData>(
          *web_contents),
      task_id_(std::move(task_id)),
      thread_url_(std::move(thread_url)) {}

ContextualTasksTabRestoreData::~ContextualTasksTabRestoreData() = default;

// static
void ContextualTasksTabRestoreData::RestoreFromExtraData(
    content::WebContents* web_contents,
    const std::map<std::string, std::string>& extra_data) {
  if (!ShouldRestoreSidePanelOnTabRestore()) {
    return;
  }
  auto task_it = extra_data.find(kTaskIdKey);
  if (task_it == extra_data.end()) {
    return;
  }
  base::Uuid task_id = base::Uuid::ParseLowercase(task_it->second);
  if (!task_id.is_valid()) {
    return;
  }
  GURL thread_url;
  if (auto url_it = extra_data.find(kThreadUrlKey);
      url_it != extra_data.end()) {
    thread_url = GURL(url_it->second);
  }
  CreateForWebContents(web_contents, std::move(task_id), std::move(thread_url));
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ContextualTasksTabRestoreData);

}  // namespace contextual_tasks
