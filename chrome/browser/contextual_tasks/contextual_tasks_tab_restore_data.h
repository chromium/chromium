// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_TAB_RESTORE_DATA_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_TAB_RESTORE_DATA_H_

#include <map>
#include <string>

#include "base/uuid.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/gurl.h"

namespace contextual_tasks {

// Holds the side panel state saved with a recently closed tab, from when the
// reopened tab is created until it's added to a browser window.
class ContextualTasksTabRestoreData
    : public content::WebContentsUserData<ContextualTasksTabRestoreData> {
 public:
  // Keys for the side panel state in the extra data of a closed tab.
  static constexpr char kTaskIdKey[] = "contextual_tasks.task_id";
  static constexpr char kThreadUrlKey[] = "contextual_tasks.thread_url";

  ~ContextualTasksTabRestoreData() override;

  // Attaches the side panel state in `extra_data` to `web_contents`, if any.
  // Does nothing unless `ShouldRestoreSidePanelOnTabRestore()` is true.
  static void RestoreFromExtraData(
      content::WebContents* web_contents,
      const std::map<std::string, std::string>& extra_data);

  const base::Uuid& task_id() const { return task_id_; }
  const GURL& thread_url() const { return thread_url_; }

 private:
  friend class content::WebContentsUserData<ContextualTasksTabRestoreData>;

  ContextualTasksTabRestoreData(content::WebContents* web_contents,
                                base::Uuid task_id,
                                GURL thread_url);

  // The task that the tab was associated with.
  base::Uuid task_id_;

  // The URL of the page that the panel showed, like an AI thread or a Lens
  // search. Loaded again in the reopened tab's panel, and used to recreate the
  // task if it was deleted after the tab closed.
  GURL thread_url_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_TAB_RESTORE_DATA_H_
