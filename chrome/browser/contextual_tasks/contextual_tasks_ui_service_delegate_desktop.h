// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UI_SERVICE_DELEGATE_DESKTOP_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UI_SERVICE_DELEGATE_DESKTOP_H_

#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_delegate.h"
#include "url/gurl.h"

class BrowserWindowInterface;
class Profile;

namespace contextual_tasks {
class ContextualTasksUiServiceDelegateDesktop
    : public ContextualTasksUiServiceDelegate {
 public:
  explicit ContextualTasksUiServiceDelegateDesktop(Profile* profile);
  ~ContextualTasksUiServiceDelegateDesktop() override;

  ContextualTasksUiServiceDelegateDesktop(
      const ContextualTasksUiServiceDelegateDesktop&) = delete;
  ContextualTasksUiServiceDelegateDesktop& operator=(
      const ContextualTasksUiServiceDelegateDesktop&) = delete;

  // ContextualTasksUiServiceDelegate overrides:
  void OpenFeedbackUi(BrowserWindowInterface* browser,
                      const GURL& page_url) override;
  void ShowUndoSnackbar(
      BrowserWindowInterface* browser_window_interface) override;
  void OnWebUIReady(BrowserWindowInterface* browser_window_interface,
                    const base::Uuid& task_id,
                    content::WebContents* web_contents) override;
  void OnWebUIDestroyed(BrowserWindowInterface* browser_window_interface,
                        const std::optional<base::Uuid>& task_id) override;
  void OnTaskChanged(BrowserWindowInterface* browser_window_interface,
                     const std::optional<base::Uuid>& old_task_id,
                     const std::optional<base::Uuid>& new_task_id) override;
  void StartPlatformVoiceRecognition(
      BrowserWindowInterface* browser_window_interface) override;

 protected:
  Profile* profile() const { return profile_; }

 private:
  raw_ptr<Profile> profile_;
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UI_SERVICE_DELEGATE_DESKTOP_H_
