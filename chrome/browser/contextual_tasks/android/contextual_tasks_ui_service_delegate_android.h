// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_ANDROID_CONTEXTUAL_TASKS_UI_SERVICE_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_ANDROID_CONTEXTUAL_TASKS_UI_SERVICE_DELEGATE_ANDROID_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
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

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_ANDROID_CONTEXTUAL_TASKS_UI_SERVICE_DELEGATE_ANDROID_H_
