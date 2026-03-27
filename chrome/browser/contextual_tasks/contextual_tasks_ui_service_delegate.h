// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UI_SERVICE_DELEGATE_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UI_SERVICE_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "url/gurl.h"

class BrowserWindowInterface;
class Profile;

namespace contextual_tasks {

// An interface to handle platform specific implementations of
// ContextualTasksUiService.
class ContextualTasksUiServiceDelegate {
 public:
  explicit ContextualTasksUiServiceDelegate(Profile* profile)
      : profile_(profile) {}
  virtual ~ContextualTasksUiServiceDelegate() = default;

  ContextualTasksUiServiceDelegate(const ContextualTasksUiServiceDelegate&) =
      delete;
  ContextualTasksUiServiceDelegate& operator=(
      const ContextualTasksUiServiceDelegate&) = delete;

  // Helper in OpenHelpUi().
  virtual void OpenHelpUi(BrowserWindowInterface* browser,
                          const GURL& page_url) = 0;

 protected:
  Profile* profile() const { return profile_; }

 private:
  raw_ptr<Profile> profile_;
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UI_SERVICE_DELEGATE_H_
