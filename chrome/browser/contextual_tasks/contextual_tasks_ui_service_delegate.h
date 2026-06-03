// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UI_SERVICE_DELEGATE_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UI_SERVICE_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "base/uuid.h"
#include "url/gurl.h"

class BrowserWindowInterface;

namespace content {
class WebContents;
}

namespace contextual_tasks {

// An interface to handle platform specific implementations of
// ContextualTasksUiService.
class ContextualTasksUiServiceDelegate {
 public:
  virtual ~ContextualTasksUiServiceDelegate() = default;

  // Called to open the feedback page UI.
  virtual void OpenFeedbackUi(BrowserWindowInterface* browser,
                              const GURL& page_url) = 0;

  // Called to show the undo closure snackbar.
  virtual void ShowUndoSnackbar(
      BrowserWindowInterface* browser_window_interface) = 0;

  // Called when the WebUI is ready.
  virtual void OnWebUIReady(BrowserWindowInterface* browser_window_interface,
                            const base::Uuid& task_id,
                            content::WebContents* web_contents) = 0;

  // Called when the WebUI controller is destroyed.
  virtual void OnWebUIDestroyed(
      BrowserWindowInterface* browser_window_interface,
      const std::optional<base::Uuid>& task_id) = 0;

  // Called when the task ID is updated.
  virtual void OnTaskChanged(BrowserWindowInterface* browser_window_interface,
                             const std::optional<base::Uuid>& old_task_id,
                             const std::optional<base::Uuid>& new_task_id) = 0;

  // Called to invoke the platform's native voice recognition system.
  virtual void StartPlatformVoiceRecognition(
      BrowserWindowInterface* browser_window_interface) = 0;
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UI_SERVICE_DELEGATE_H_
