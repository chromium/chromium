// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/editor_system_actuator.h"

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

EditorSystemActuator::EditorSystemActuator(
    Profile* profile,
    mojo::PendingAssociatedReceiver<orca::mojom::SystemActuator> receiver,
    System* system)
    : profile_(profile),
      system_actuator_receiver_(this, std::move(receiver)),
      system_(system) {}

EditorSystemActuator::~EditorSystemActuator() = default;

void EditorSystemActuator::InsertText(const std::string& text) {
  EditorMetricsRecorder* logger = system_->GetMetricsRecorder();
  logger->LogEditorState(EditorStates::kInsert);
  logger->LogNumberOfCharactersInserted(text.length());
  logger->LogNumberOfCharactersSelectedForInsert(
      system_->GetSelectedTextLength());
  // We queue the text to be inserted here rather then insert it directly into
  // the input.
  inserter_.InsertTextOnNextFocus(text);
  system_->OnTextInsertionRequested();
}

void EditorSystemActuator::ApproveConsent() {
  system_->ProcessConsentAction(ConsentAction::kApproved);
}

void EditorSystemActuator::DeclineConsent() {
  system_->ProcessConsentAction(ConsentAction::kDeclined);
}

void EditorSystemActuator::OpenUrlInNewWindow(const GURL& url) {
  if (!IsUrlAllowed(url)) {
    mojo::ReportBadMessage("Invalid URL scheme. Only HTTPS is allowed.");
    return;
  }
  ash::NewWindowDelegate::GetPrimary()->OpenUrl(
      url, ash::NewWindowDelegate::OpenUrlFrom::kUnspecified,
      ash::NewWindowDelegate::Disposition::kNewForegroundTab);
}

void EditorSystemActuator::ShowUI() {
  system_->ShowUI();
}

void EditorSystemActuator::CloseUI() {
  system_->GetMetricsRecorder()->LogEditorState(
      EditorStates::kClickCloseButton);
  system_->CloseUI();
}

void EditorSystemActuator::SubmitFeedback(const std::string& description) {
  SendEditorFeedback(profile_, description);
}

void EditorSystemActuator::OnFocus(int context_id) {
  inserter_.OnFocus(context_id);
}

void EditorSystemActuator::OnBlur() {
  inserter_.OnBlur();
}

}  // namespace ash::input_method
