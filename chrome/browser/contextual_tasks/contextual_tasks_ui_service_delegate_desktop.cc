// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_delegate_desktop.h"

#include "chrome/browser/feedback/public/feedback_source.h"
#include "chrome/browser/feedback/show_feedback_page.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/grit/branded_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace contextual_tasks {

ContextualTasksUiServiceDelegateDesktop::
    ContextualTasksUiServiceDelegateDesktop(Profile* profile)
    : ContextualTasksUiServiceDelegate() {}

ContextualTasksUiServiceDelegateDesktop::
    ~ContextualTasksUiServiceDelegateDesktop() = default;

void ContextualTasksUiServiceDelegateDesktop::OpenFeedbackUi(
    BrowserWindowInterface* browser,
    const GURL& page_url) {
  chrome::ShowFeedbackPage(page_url, profile(), feedback::kFeedbackSourceAI,
                           /*description_template=*/std::string(),
                           /*description_placeholder_text=*/
                           l10n_util::GetStringUTF8(IDS_LENS_SEND_FEEDBACK),
                           /*category_tag=*/"cobrowse",
                           /*extra_diagnostics=*/std::string());
}

void ContextualTasksUiServiceDelegateDesktop::ShowUndoSnackbar(
    BrowserWindowInterface* browser_window_interface) {}

void ContextualTasksUiServiceDelegateDesktop::OnWebUIReady(
    BrowserWindowInterface* browser_window_interface,
    const base::Uuid& task_id,
    content::WebContents* web_contents) {}

void ContextualTasksUiServiceDelegateDesktop::OnWebUIDestroyed(
    BrowserWindowInterface* browser_window_interface,
    const std::optional<base::Uuid>& task_id) {}

void ContextualTasksUiServiceDelegateDesktop::OnTaskChanged(
    BrowserWindowInterface* browser_window_interface,
    const std::optional<base::Uuid>& old_task_id,
    const std::optional<base::Uuid>& new_task_id) {}

void ContextualTasksUiServiceDelegateDesktop::StartPlatformVoiceRecognition(
    BrowserWindowInterface* browser_window_interface) {}

}  // namespace contextual_tasks
