// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/mahi/mahi_ui_controller.h"

#include <memory>

#include "ash/system/mahi/mahi_constants.h"
#include "ash/system/mahi/mahi_panel_drag_controller.h"
#include "ash/system/mahi/mahi_panel_widget.h"
#include "ash/system/mahi/mahi_ui_update.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"
#include "ui/views/view.h"

namespace ash {

namespace {

// Returns true if `status` indicates an error.
// NOTE: `chromeos::MahiResponseStatus::kLowQuota` is a warning.
bool IsErrorStatus(chromeos::MahiResponseStatus status) {
  switch (status) {
    case chromeos::MahiResponseStatus::kCantFindOutputData:
    case chromeos::MahiResponseStatus::kContentExtractionError:
    case chromeos::MahiResponseStatus::kInappropriate:
    case chromeos::MahiResponseStatus::kQuotaLimitHit:
    case chromeos::MahiResponseStatus::kResourceExhausted:
    case chromeos::MahiResponseStatus::kUnknownError:
      return true;
    case chromeos::MahiResponseStatus::kLowQuota:
    case chromeos::MahiResponseStatus::kSuccess:
      return false;
  }
}

}  // namespace

// MahiUiController::delegate --------------------------------------------------

MahiUiController::Delegate::Delegate(MahiUiController* ui_controller) {
  CHECK(ui_controller);
  observation_.Observe(ui_controller);
}

MahiUiController::Delegate::~Delegate() = default;

// MahiUiController ------------------------------------------------------------

MahiUiController::MahiUiController()
    : drag_controller_(std::make_unique<MahiPanelDragController>(this)) {}

MahiUiController::~MahiUiController() {
  if (mahi_panel_widget_) {
    // Immediately close the widget to avoid dangling pointers in tests.
    mahi_panel_widget_->CloseNow();
    mahi_panel_widget_.reset();
  }
}

void MahiUiController::AddDelegate(Delegate* delegate) {
  delegates_.AddObserver(delegate);
}

void MahiUiController::RemoveDelegate(Delegate* delegate) {
  delegates_.RemoveObserver(delegate);
}

void MahiUiController::OpenMahiPanel(int64_t display_id) {
  // TODO(http://b/339250208): Use DCHECK instead of return early when
  // `IsEnabled()` is false.
  if (!chromeos::MahiManager::Get()->IsEnabled()) {
    return;
  }

  mahi_panel_widget_ =
      MahiPanelWidget::CreatePanelWidget(display_id, /*ui_controller=*/this);
  mahi_panel_widget_->Show();
}

void MahiUiController::CloseMahiPanel() {
  mahi_panel_widget_.reset();
}

bool MahiUiController::IsMahiPanelOpen() {
  return !!mahi_panel_widget_;
}

void MahiUiController::NavigateToQuestionAnswerView() {
  SetVisibilityStateAndNotifyUiUpdate(
      VisibilityState::kQuestionAndAnswer,
      MahiUiUpdate(MahiUiUpdateType::kQuestionAndAnswerViewNavigated));
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
  most_recent_question_params_.reset();
  NavigateToSummaryOutlinesSection();
  NotifyUiUpdate(MahiUiUpdate(MahiUiUpdateType::kContentsRefreshInitiated));
}

void MahiUiController::Retry(VisibilityState origin_state) {
  switch (origin_state) {
    case VisibilityState::kQuestionAndAnswer:
      if (most_recent_question_params_) {
        SetVisibilityStateAndNotifyUiUpdate(
            origin_state, MahiUiUpdate(MahiUiUpdateType::kQuestionReAsked,
                                       *most_recent_question_params_));
      } else {
        LOG(ERROR) << "Tried to re-ask a non-existing question";
      }
      return;
    case VisibilityState::kSummaryAndOutlines:
      SetVisibilityStateAndNotifyUiUpdate(
          origin_state,
          MahiUiUpdate(MahiUiUpdateType::kSummaryAndOutlinesReloaded));
      return;
    case VisibilityState::kError:
      NOTREACHED_NORETURN();
  }
}

void MahiUiController::SendQuestion(const std::u16string& question,
                                    bool current_panel_content,
                                    QuestionSource source) {
  base::UmaHistogramEnumeration(
      mahi_constants::kMahiQuestionSourceHistogramName, source);

  if (source != QuestionSource::kRetry) {
    most_recent_question_params_.emplace(question, current_panel_content);
  }

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

void MahiUiController::HandleError(const MahiUiError& error) {
  // `chromeos::MahiResponseStatus::kLowQuota` is a warning not an error.
  CHECK_NE(error.status, chromeos::MahiResponseStatus::kLowQuota);

  // The presentation of the inappropriate error during
  // `State::kQuestionAndAnswer` should be embedded into the Q&A view instead
  // of a separate view.
  const MahiUiUpdate update(MahiUiUpdateType::kErrorReceived, error);
  if (error.status == chromeos::MahiResponseStatus::kInappropriate &&
      error.origin_state == VisibilityState::kQuestionAndAnswer) {
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
  if (IsErrorStatus(status)) {
    HandleError(MahiUiError(
        status, /*origin_state=*/VisibilityState::kQuestionAndAnswer));
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
  if (IsErrorStatus(status)) {
    HandleError(MahiUiError(
        status, /*origin_state=*/VisibilityState::kSummaryAndOutlines));
    return;
  }

  NotifyUiUpdate(MahiUiUpdate(MahiUiUpdateType::kOutlinesLoaded, outlines));
}

void MahiUiController::OnSummaryLoaded(std::u16string summary_text,
                                       chromeos::MahiResponseStatus status) {
  if (IsErrorStatus(status)) {
    HandleError(MahiUiError(
        status, /*origin_state=*/VisibilityState::kSummaryAndOutlines));
    return;
  }

  NotifyUiUpdate(MahiUiUpdate(MahiUiUpdateType::kSummaryLoaded, summary_text));
}

}  // namespace ash
