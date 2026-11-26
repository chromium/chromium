// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_delegate_desktop.h"

#include "chrome/browser/feedback/public/feedback_source.h"
#include "chrome/browser/feedback/show_feedback_page.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_dogfood_feedback.h"
#include "chrome/grit/branded_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace contextual_tasks {

namespace {

// Identifier of the gFeedback bucket that Contextual Tasks reports are filed
// in.
constexpr char kFeedbackCategoryTag[] = "cobrowse";

// Identifier of the gFeedback bucket that Contextual Tasks dogfood reports are
// filed in.
constexpr char kDogfoodFeedbackCategoryTag[] = "contextual_tasks_rearch";

// Dogfood link placed at the top of the dogfood feedback description template.
constexpr char kDogfoodDescriptionHeader[] = "go/sic-rearch-dogfood";

}  // namespace

ContextualTasksUiServiceDelegateDesktop::
    ContextualTasksUiServiceDelegateDesktop(Profile* profile)
    : ContextualTasksUiServiceDelegate(), profile_(profile) {}

ContextualTasksUiServiceDelegateDesktop::
    ~ContextualTasksUiServiceDelegateDesktop() = default;

void ContextualTasksUiServiceDelegateDesktop::OpenFeedbackUi(
    BrowserWindowInterface* browser,
    const GURL& page_url) {
  chrome::ShowFeedbackPage(page_url, profile(), feedback::kFeedbackSourceAI,
                           /*description_template=*/std::string(),
                           /*description_placeholder_text=*/
                           l10n_util::GetStringUTF8(IDS_LENS_SEND_FEEDBACK),
                           /*category_tag=*/kFeedbackCategoryTag,
                           /*extra_diagnostics=*/std::string());
}

void ContextualTasksUiServiceDelegateDesktop::OpenDogfoodFeedbackUi(
    BrowserWindowInterface* browser,
    const GURL& page_url) {
  // Reuses the Omnibox dogfood form, but files reports in the Contextual Tasks
  // dogfood bucket.
  omnibox_dogfood_feedback::ShowFeedbackForm(
      profile(), page_url, kDogfoodDescriptionHeader,
      feedback::kFeedbackSourceContextualTasks, kDogfoodFeedbackCategoryTag);
}

void ContextualTasksUiServiceDelegateDesktop::ShowUndoSnackbar(
    BrowserWindowInterface* browser_window_interface) {}

void ContextualTasksUiServiceDelegateDesktop::StartPlatformVoiceRecognition(
    BrowserWindowInterface* browser_window_interface) {}

}  // namespace contextual_tasks
