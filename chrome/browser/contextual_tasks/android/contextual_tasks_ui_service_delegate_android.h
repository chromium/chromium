// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_ANDROID_CONTEXTUAL_TASKS_UI_SERVICE_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_ANDROID_CONTEXTUAL_TASKS_UI_SERVICE_DELEGATE_ANDROID_H_

#include "base/android/scoped_java_ref.h"
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

  // Called from Java via JNI to undo the closure of the sheet.
  void UndoClose(JNIEnv* env, int64_t browser_window_ptr);

  // ContextualTasksUiServiceDelegate overrides:
  void OpenFeedbackUi(BrowserWindowInterface* browser,
                      const GURL& page_url) override;
  void ShowUndoSnackbar(
      BrowserWindowInterface* browser_window_interface) override;
  void OnWebUIReady(const base::Uuid& task_id,
                    content::WebContents* web_contents) override;

 protected:
  Profile* profile() const { return profile_; }

 private:
  raw_ptr<Profile> profile_;
  base::android::ScopedJavaGlobalRef<jobject> java_delegate_;
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_ANDROID_CONTEXTUAL_TASKS_UI_SERVICE_DELEGATE_ANDROID_H_
