// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_TASK_INFO_DELEGATE_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_TASK_INFO_DELEGATE_H_

#include <optional>
#include <string>

#include "base/uuid.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"

namespace contextual_tasks {

// An interface for managing task information held by the WebUI. Primarily used
// by the FrameNavObserver to update the state of the AI thread.
class TaskInfoDelegate {
 public:
  virtual ~TaskInfoDelegate() = default;

  virtual const std::optional<base::Uuid>& GetTaskId() = 0;
  virtual void SetTaskId(std::optional<base::Uuid> id) = 0;
  virtual const std::optional<std::string>& GetThreadId() = 0;
  virtual void SetThreadId(std::optional<std::string> id) = 0;
  virtual void SetThreadTurnId(std::optional<std::string> id) = 0;
  virtual const std::optional<std::string>& GetThreadTitle() = 0;
  virtual void SetThreadTitle(std::optional<std::string> title) = 0;
  virtual void SetAimUrl(const GURL& url) = 0;
  virtual void SetIsAiPage(bool is_ai_page) = 0;
  virtual bool IsShownInTab() = 0;
  virtual BrowserWindowInterface* GetBrowser() = 0;
  virtual content::WebContents* GetWebUIWebContents() = 0;
  virtual void OnZeroStateChange(bool is_zero_state) = 0;

  // Get aim URL preserved in contextual tasks URL search params.
  virtual GURL GetAimUrl() = 0;

  // Called to prepare for a task change.
  virtual void PrepareForTaskChange() = 0;

  // Called when the task has been changed.
  virtual void OnTaskChanged() = 0;
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_TASK_INFO_DELEGATE_H_
