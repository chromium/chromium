// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/android/contextual_tasks_ui_service_delegate_android.h"

#include "chrome/browser/contextual_tasks/android/contextual_tasks_bridge.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_utils.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "url/gurl.h"

namespace contextual_tasks {

ContextualTasksUiServiceDelegateAndroid::
    ContextualTasksUiServiceDelegateAndroid(Profile* profile)
    : ContextualTasksUiServiceDelegate(), profile_(profile) {}

ContextualTasksUiServiceDelegateAndroid::
    ~ContextualTasksUiServiceDelegateAndroid() = default;

void ContextualTasksUiServiceDelegateAndroid::OpenFeedbackUi(
    BrowserWindowInterface* browser_window_interface,
    const GURL& page_url) {
  auto* bridge = ContextualTasksBridge::From(browser_window_interface);
  if (!bridge) {
    return;
  }
  bridge->NotifyOpenFeedbackUi(page_url);
}

void ContextualTasksUiServiceDelegateAndroid::OnWebUIReady(
    BrowserWindowInterface* browser_window_interface,
    const base::Uuid& task_id,
    content::WebContents* web_contents) {
  auto* bridge = ContextualTasksBridge::From(browser_window_interface);
  if (!bridge) {
    return;
  }
  bridge->NotifyWebUIReady(task_id, web_contents);
}

void ContextualTasksUiServiceDelegateAndroid::OnWebUIDestroyed(
    BrowserWindowInterface* browser_window_interface,
    const std::optional<base::Uuid>& task_id) {
  auto* bridge = ContextualTasksBridge::From(browser_window_interface);
  if (!bridge) {
    return;
  }
  bridge->NotifyWebUIDestroyed(task_id);
}

void ContextualTasksUiServiceDelegateAndroid::OnTaskChanged(
    BrowserWindowInterface* browser_window_interface,
    const std::optional<base::Uuid>& old_task_id,
    const std::optional<base::Uuid>& new_task_id) {
  auto* bridge = ContextualTasksBridge::From(browser_window_interface);
  if (!bridge) {
    return;
  }
  bridge->NotifyTaskChanged(old_task_id, new_task_id);
}

void ContextualTasksUiServiceDelegateAndroid::ShowUndoSnackbar(
    BrowserWindowInterface* browser_window_interface) {
  if (ShouldShowSidePanel()) {
    return;
  }

  auto* bridge = ContextualTasksBridge::From(browser_window_interface);
  if (!bridge) {
    return;
  }
  bridge->NotifyShowUndoSnackbar();
}

void ContextualTasksUiServiceDelegateAndroid::StartPlatformVoiceRecognition(
    BrowserWindowInterface* browser_window_interface) {
  auto* bridge = ContextualTasksBridge::From(browser_window_interface);
  if (!bridge) {
    return;
  }
  bridge->StartPlatformVoiceRecognition();
}

}  // namespace contextual_tasks
