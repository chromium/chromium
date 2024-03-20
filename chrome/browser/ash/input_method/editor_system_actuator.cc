// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/editor_system_actuator.h"

#include <memory>

#include "ash/public/cpp/new_window_delegate.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/input_method/editor_feedback.h"
#include "chrome/browser/ash/input_method/editor_metrics_enums.h"
#include "chrome/browser/ash/input_method/editor_metrics_recorder.h"
#include "chrome/browser/ash/input_method/editor_text_insertion.h"
#include "chrome/browser/ash/input_method/url_utils.h"
#include "url/url_constants.h"

namespace ash::input_method {
namespace {

constexpr base::TimeDelta kAnnouncementDelay = base::Milliseconds(200);
constexpr char16_t kAnnouncementForFeedback[] = u"Feedback submitted";
constexpr char16_t kAnnouncementForInsertion[] =
    u"Replacing selected text with suggestion";

constexpr std::string_view
    kDomainsRequiringParagraphConcatenationWhenInsertingText[] = {
        "notion",
        "medium",
};

bool IsUrlAllowed(const GURL& url) {
  return url.SchemeIs(url::kHttpsScheme) ||
         url.spec().starts_with("chrome://os-settings/osLanguages/input");
}

EditorTextInsertion::InsertionStrategy GetInsertionStrategy(const GURL& url) {
  for (std::string_view domain :
       kDomainsRequiringParagraphConcatenationWhenInsertingText) {
    if (IsSubDomain(url, domain)) {
      return EditorTextInsertion::InsertionStrategy::kInsertAsASingleParagraph;
    }
  }
  return EditorTextInsertion::InsertionStrategy::kInsertAsMultipleParagraphs;
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
  logger->LogEditorState(EditorStates::kTextInsertionRequested);
  // After making an announcement there needs to be a small delay to ensure any
  // other announcements triggered from a text insertion do not collide with the
  // original announcement.
  system_->Announce(kAnnouncementForInsertion);
  announcement_delay_.Reset();
  announcement_delay_.Start(
      FROM_HERE, kAnnouncementDelay,
      base::BindOnce(&EditorSystemActuator::QueueTextInsertion,
                     weak_ptr_factory_.GetWeakPtr(), text));
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
  system_->Announce(kAnnouncementForFeedback);
}

void EditorSystemActuator::OnTrigger(
    orca::mojom::TriggerContextPtr trigger_context) {
  EditorMetricsRecorder* logger = system_->GetMetricsRecorder();
  logger->SetTone(ToEditorMetricTone(std::move(trigger_context)));
}

void EditorSystemActuator::EmitMetricEvent(
    orca::mojom::MetricEvent metric_event) {
  EditorMetricsRecorder* logger = system_->GetMetricsRecorder();
  std::optional<EditorStates> editor_state_metric =
      ToEditorStatesMetric(metric_event);

  if (editor_state_metric.has_value()) {
    logger->LogEditorState(editor_state_metric.value());
  }
}

void EditorSystemActuator::OnFocus(int context_id) {
  if (queued_text_insertion_ == nullptr) {
    return;
  }
  if (queued_text_insertion_->HasTimedOut()) {
    queued_text_insertion_ = nullptr;
    return;
  }
  if (queued_text_insertion_->Commit()) {
    EditorMetricsRecorder* logger = system_->GetMetricsRecorder();
    logger->LogEditorState(EditorStates::kInsert);
    logger->LogNumberOfCharactersInserted(
        queued_text_insertion_->GetTextLength());
    logger->LogNumberOfCharactersSelectedForInsert(
        system_->GetSelectedTextLength());
    queued_text_insertion_ = nullptr;
    return;
  }
}

void EditorSystemActuator::QueueTextInsertion(const std::string pending_text) {
  // The text cannot be immediately inserted as the target input is not focused
  // at this point, the WebUI is focused. After closing the WebUI focus will
  // return to the original text input.
  queued_text_insertion_ = std::make_unique<EditorTextInsertion>(
      std::move(pending_text), GetInsertionStrategy(current_url_));
  EditorMetricsRecorder* logger = system_->GetMetricsRecorder();
  logger->LogEditorState(EditorStates::kTextQueuedForInsertion);
  system_->CloseUI();
}

void EditorSystemActuator::OnInputContextUpdated(const GURL& url) {
  current_url_ = url;
}

}  // namespace ash::input_method
