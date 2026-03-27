// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UI_SERVICE_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UI_SERVICE_DELEGATE_ANDROID_H_

#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_delegate.h"
#include "url/gurl.h"

class BrowserWindowInterface;
class Profile;

namespace contextual_tasks {
class ContextualTasksUiServiceDelegateAndroid
    : public ContextualTasksUiServiceDelegate {
 public:
  explicit ContextualTasksUiServiceDelegateAndroid(Profile* profile);
  ~ContextualTasksUiServiceDelegateAndroid() override;

  ContextualTasksUiServiceDelegateAndroid(
      const ContextualTasksUiServiceDelegateAndroid&) = delete;
  ContextualTasksUiServiceDelegateAndroid& operator=(
      const ContextualTasksUiServiceDelegateAndroid&) = delete;

  // ContextualTasksUiServiceDelegate overrides:
  void OpenHelpUi(BrowserWindowInterface* browser,
                  const GURL& page_url) override;
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UI_SERVICE_DELEGATE_ANDROID_H_
