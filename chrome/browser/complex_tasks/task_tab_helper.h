// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPLEX_TASKS_TASK_TAB_HELPER_H_
#define CHROME_BROWSER_COMPLEX_TASKS_TASK_TAB_HELPER_H_

#include <map>
#include <unordered_map>

#include "base/containers/contains.h"
#include "build/build_config.h"
#include "components/sessions/content/navigation_task_id.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace sessions {
class NavigationTaskId;
}

namespace tasks {

// This is a tab helper that collects navigation state information of a
// complex task.
class TaskTabHelper : public content::WebContentsObserver,
                      public content::WebContentsUserData<TaskTabHelper> {
 public:
  TaskTabHelper(const TaskTabHelper&) = delete;
  TaskTabHelper& operator=(const TaskTabHelper&) = delete;

  ~TaskTabHelper() override;

  // WebContentsObserver
  void NavigationEntryCommitted(
      const content::LoadCommittedDetails& load_details) override;
  static sessions::NavigationTaskId* GetCurrentTaskId(
      content::WebContents* web_contents);
  const sessions::NavigationTaskId* get_task_id_for_navigation(
      int nav_id) const {
    if (!base::Contains(local_navigation_task_id_map_, nav_id))
      return nullptr;
    return &local_navigation_task_id_map_.find(nav_id)->second;
  }

 protected:
  explicit TaskTabHelper(content::WebContents* web_contents);

 private:
  friend class content::WebContentsUserData<TaskTabHelper>;
  void UpdateAndRecordTaskIds(
      const content::LoadCommittedDetails& load_details);

  int64_t GetParentTaskId();
  int64_t GetParentRootTaskId();

  std::unordered_map<int, sessions::NavigationTaskId>
      local_navigation_task_id_map_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace tasks

#endif  // CHROME_BROWSER_COMPLEX_TASKS_TASK_TAB_HELPER_H_
