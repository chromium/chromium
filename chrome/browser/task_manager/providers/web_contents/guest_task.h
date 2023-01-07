// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_GUEST_TASK_H_
#define CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_GUEST_TASK_H_

#include "chrome/browser/task_manager/providers/web_contents/renderer_task.h"

namespace task_manager {

// Defines a concrete RendererTask that represents WebContents owned by the
// GuestViewBase which represents a <*view> tag that is a browser plugin guest.
class GuestTask : public RendererTask {
 public:
  explicit GuestTask(content::WebContents* web_contents);
  GuestTask(const GuestTask&) = delete;
  GuestTask& operator=(const GuestTask&) = delete;
  ~GuestTask() override;

  // task_manager::RendererTask:
  void UpdateTitle() override;
  void UpdateFavicon() override;
  Type GetType() const override;

 private:
  std::u16string GetCurrentTitle(content::WebContents* web_contents) const;
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WEB_CONTENTS_GUEST_TASK_H_
