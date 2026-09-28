// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPLEX_TASKS_TASK_TAB_HELPER_H_
#define CHROME_BROWSER_COMPLEX_TASKS_TASK_TAB_HELPER_H_

#include <unordered_map>

#include "build/build_config.h"
#include "components/sessions/content/navigation_task_id.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace sessions {
class NavigationTaskId;
}

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace tasks {

// This is a tab helper that collects navigation state information of a
// complex task.
class TaskTabHelper : public content::WebContentsObserver {
 public:
  DECLARE_USER_DATA(TaskTabHelper);

  TaskTabHelper(tabs::TabInterface& tab, content::WebContents* web_contents);
  TaskTabHelper(const TaskTabHelper&) = delete;
  TaskTabHelper& operator=(const TaskTabHelper&) = delete;

  ~TaskTabHelper() override;

  static TaskTabHelper* From(tabs::TabInterface* tab);

  // WebContentsObserver
  void NavigationEntryCommitted(
      const content::LoadCommittedDetails& load_details) override;
  static sessions::NavigationTaskId* GetCurrentTaskId(
      content::WebContents* web_contents);
  const sessions::NavigationTaskId* get_task_id_for_navigation(
      int nav_id) const {
    if (!local_navigation_task_id_map_.contains(nav_id)) {
      return nullptr;
    }
    return &local_navigation_task_id_map_.find(nav_id)->second;
  }

 private:
  void UpdateAndRecordTaskIds(
      const content::LoadCommittedDetails& load_details);

  int64_t GetParentTaskId();
  int64_t GetParentRootTaskId();

  std::unordered_map<int, sessions::NavigationTaskId>
      local_navigation_task_id_map_;

  ui::ScopedUnownedUserData<TaskTabHelper> scoped_unowned_user_data_;
};

}  // namespace tasks

#endif  // CHROME_BROWSER_COMPLEX_TASKS_TASK_TAB_HELPER_H_
