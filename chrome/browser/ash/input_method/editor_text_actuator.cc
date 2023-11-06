// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/editor_text_actuator.h"

#include "ash/public/cpp/new_window_delegate.h"
#include "chrome/browser/ash/input_method/editor_feedback.h"
#include "chrome/browser/ash/input_method/editor_metrics_enums.h"
#include "chrome/browser/ash/input_method/editor_metrics_recorder.h"
#include "url/url_constants.h"

namespace ash::input_method {
namespace {

bool IsUrlAllowed(const GURL& url) {
  return url.SchemeIs(url::kHttpsScheme) ||
         url.spec().starts_with("chrome://os-settings/osLanguages/input");
}

}  // namespace

EditorTextActuator::EditorTextActuator(
    Profile* profile,
    mojo::PendingAssociatedReceiver<orca::mojom::TextActuator> receiver,
    Delegate* delegate)
    : profile_(profile),
      text_actuator_receiver_(this, std::move(receiver)),
      delegate_(delegate) {}

EditorTextActuator::~EditorTextActuator() = default;

void EditorTextActuator::InsertText(const std::string& text) {
  EditorMode editor_mode = delegate_->GetEditorMode();
  LogEditorState(EditorStates::kInsert, editor_mode);
  LogNumberOfCharactersInserted(editor_mode, text.length());
  LogNumberOfCharactersSelectedForInsert(editor_mode,
                                         delegate_->GetSelectedTextLength());
  // We queue the text to be inserted here rather then insert it directly into
  // the input.
  inserter_.InsertTextOnNextFocus(text);
  delegate_->OnTextInserted();
}

void EditorTextActuator::ApproveConsent() {
  delegate_->ProcessConsentAction(ConsentAction::kApproved);
}

void EditorTextActuator::DeclineConsent() {
  delegate_->ProcessConsentAction(ConsentAction::kDeclined);
}

void EditorTextActuator::OpenUrlInNewWindow(const GURL& url) {
  if (!IsUrlAllowed(url)) {
    mojo::ReportBadMessage("Invalid URL scheme. Only HTTPS is allowed.");
    return;
  }
  ash::NewWindowDelegate::GetPrimary()->OpenUrl(
      url, ash::NewWindowDelegate::OpenUrlFrom::kUnspecified,
      ash::NewWindowDelegate::Disposition::kNewForegroundTab);
}

void EditorTextActuator::ShowUI() {
  delegate_->ShowUI();
}

void EditorTextActuator::CloseUI() {
  LogEditorState(EditorStates::kClickCloseButton, delegate_->GetEditorMode());
  delegate_->CloseUI();
}

void EditorTextActuator::SubmitFeedback(const std::string& description) {
  SendEditorFeedback(profile_, description);
}

void EditorTextActuator::OnFocus(int context_id) {
  inserter_.OnFocus(context_id);
}

void EditorTextActuator::OnBlur() {
  inserter_.OnBlur();
}

}  // namespace ash::input_method
