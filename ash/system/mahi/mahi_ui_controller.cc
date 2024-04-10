// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/mahi/mahi_ui_controller.h"

#include "base/logging.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"
#include "ui/views/view.h"

namespace ash {

namespace {

// Returns true if `status` indicates an error.
bool HasError(chromeos::MahiResponseStatus status) {
  return status != chromeos::MahiResponseStatus::kSuccess;
}

}  // namespace

// MahiUiController::delegate --------------------------------------------------

MahiUiController::Delegate::Delegate(MahiUiController* ui_controller) {
  CHECK(ui_controller);
  observation_.Observe(ui_controller);
}

MahiUiController::Delegate::~Delegate() = default;

// MahiUiController ------------------------------------------------------------

MahiUiController::MahiUiController() = default;

MahiUiController::~MahiUiController() = default;

void MahiUiController::AddDelegate(Delegate* delegate) {
  delegates_.AddObserver(delegate);
}

void MahiUiController::RemoveDelegate(Delegate* delegate) {
  delegates_.RemoveObserver(delegate);
}

void MahiUiController::NavigateToSummaryOutlinesSection() {
  SetVisibilityStateAndNotifyUiUpdate(
      VisibilityState::kSummaryAndOutlines,
      MahiUiUpdate(MahiUiUpdateType::kSummaryAndOutlinesSectionNavigated));
}

void MahiUiController::NotifyRefreshAvailabilityChanged(bool available) {
  NotifyUiUpdate(
      MahiUiUpdate(MahiUiUpdateType::kRefreshAvailabilityUpdated, available));
}

void MahiUiController::RefreshContents() {
  NavigateToSummaryOutlinesSection();
  NotifyUiUpdate(MahiUiUpdate(MahiUiUpdateType::kContentsRefreshInitiated));
}

void MahiUiController::SendQuestion(const std::u16string& question,
                                    bool current_panel_content) {
  // Display the Q&A section.
  SetVisibilityStateAndNotifyUiUpdate(
      VisibilityState::kQuestionAndAnswer,
      MahiUiUpdate(MahiUiUpdateType::kQuestionPosted, question));

  chromeos::MahiManager::Get()->AnswerQuestion(
      question, current_panel_content,
      base::BindOnce(&MahiUiController::OnAnswerLoaded,
                     weak_ptr_factory_.GetWeakPtr()));
}

void MahiUiController::UpdateSummaryAndOutlines() {
  chromeos::MahiManager::Get()->GetSummary(base::BindOnce(
      &MahiUiController::OnSummaryLoaded, weak_ptr_factory_.GetWeakPtr()));
  chromeos::MahiManager::Get()->GetOutlines(base::BindOnce(
      &MahiUiController::OnOutlinesLoaded, weak_ptr_factory_.GetWeakPtr()));
}

void MahiUiController::HandleErrorStatus(chromeos::MahiResponseStatus status) {
  CHECK(HasError(status));

  if (status == chromeos::MahiResponseStatus::kLowQuota) {
    // TODO(http://b/319731862): Add the low quota warning toast.
    return;
  }

  // The presentation of the inappropriate error during
  // `State::kQuestionAndAnswer` should be embedded into the Q&A view instead
  // of a separate view.
  const MahiUiUpdate update(MahiUiUpdateType::kErrorReceived, status);
  if (status == chromeos::MahiResponseStatus::kInappropriate &&
      visibility_state_ == VisibilityState::kQuestionAndAnswer) {
    NotifyUiUpdate(update);
    return;
  }

  // Display the view that presents the error.
  SetVisibilityStateAndNotifyUiUpdate(VisibilityState::kError, update);
}

void MahiUiController::NotifyUiUpdate(const MahiUiUpdate& update) {
  for (auto& delegate : delegates_) {
    delegate.OnUpdated(update);
  }
}

void MahiUiController::SetVisibilityStateAndNotifyUiUpdate(
    VisibilityState state,
    const MahiUiUpdate& update) {
  visibility_state_ = state;

  for (auto& delegate : delegates_) {
    views::View* const associated_view = delegate.GetView();
    if (const bool target_visible = delegate.GetViewVisibility(state);
        target_visible != associated_view->GetVisible()) {
      associated_view->SetVisible(target_visible);
    }

    delegate.OnUpdated(update);
  }
}

void MahiUiController::OnAnswerLoaded(std::optional<std::u16string> answer,
                                      chromeos::MahiResponseStatus status) {
  if (HasError(status)) {
    HandleErrorStatus(status);
    return;
  }

  // TODO(b/331302199): Handle the case that `answer` is `std::nullopt` in a
  // better way.
  if (!answer) {
    LOG(ERROR) << "Received an empty Mahi answer";
  }

  const std::u16string answer_after_process = answer.value_or(std::u16string());
  NotifyUiUpdate(
      MahiUiUpdate(MahiUiUpdateType::kAnswerLoaded, answer_after_process));
}

void MahiUiController::OnOutlinesLoaded(
    std::vector<chromeos::MahiOutline> outlines,
    chromeos::MahiResponseStatus status) {
  if (HasError(status)) {
    HandleErrorStatus(status);
    return;
  }

  NotifyUiUpdate(MahiUiUpdate(MahiUiUpdateType::kOutlinesLoaded, outlines));
}

void MahiUiController::OnSummaryLoaded(std::u16string summary_text,
                                       chromeos::MahiResponseStatus status) {
  if (HasError(status)) {
    HandleErrorStatus(status);
    return;
  }

  NotifyUiUpdate(MahiUiUpdate(MahiUiUpdateType::kSummaryLoaded, summary_text));
}

}  // namespace ash
