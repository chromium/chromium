// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/mahi/mahi_ui_controller.h"

#include <memory>

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/mahi/mahi_constants.h"
#include "ash/system/mahi/mahi_panel_widget.h"
#include "ash/system/mahi/mahi_ui_update.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"
#include "components/account_id/account_id.h"
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
    case chromeos::MahiResponseStatus::kRestrictedCountry:
    case chromeos::MahiResponseStatus::kUnsupportedLanguage:
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

MahiUiController::MahiUiController() {
  // The shell may not be available in tests if using a plain object for the UI
  // controller, which means the session will not be observed.
  if (Shell::HasInstance()) {
    Shell::Get()->session_controller()->AddObserver(this);
  }
}

MahiUiController::~MahiUiController() {
  if (Shell::HasInstance()) {
    Shell::Get()->session_controller()->RemoveObserver(this);
  }

  if (mahi_panel_widget_) {
    // Immediately close the widget to avoid dangling pointers in tests.
    mahi_panel_widget_->CloseNow();
    mahi_panel_widget_.reset();
  }

  RecordTimesPanelOpenedMetric();
}

void MahiUiController::AddDelegate(Delegate* delegate) {
  delegates_.AddObserver(delegate);
}

void MahiUiController::RemoveDelegate(Delegate* delegate) {
  delegates_.RemoveObserver(delegate);
}

void MahiUiController::OpenMahiPanel(int64_t display_id,
                                     gfx::Rect mahi_menu_bounds) {
  // TODO(http://b/339250208): Use DCHECK instead of return early when
  // `IsEnabled()` is false.
  if (!chromeos::MahiManager::Get()->IsEnabled()) {
    return;
  }

  mahi_panel_widget_ = MahiPanelWidget::CreateAndShowPanelWidget(
      display_id, mahi_menu_bounds, /*ui_controller=*/this);
  times_panel_opened_per_session_++;
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
      NOTREACHED();
  }
}

void MahiUiController::SendQuestion(const std::u16string& question,
                                    bool current_panel_content,
                                    QuestionSource source,
                                    bool update_summary_after_answer_question) {
  InvalidatePendingRequests();

  update_summary_after_answer_question_ = update_summary_after_answer_question;

  base::UmaHistogramEnumeration(
      mahi_constants::kMahiQuestionSourceHistogramName, source);

  if (source != QuestionSource::kRetry) {
    most_recent_question_params_.emplace(question, current_panel_content);
  }

  // Display the Q&A section.
  SetVisibilityStateAndNotifyUiUpdate(
      VisibilityState::kQuestionAndAnswer,
      MahiUiUpdate(MahiUiUpdateType::kQuestionPosted, question));

  // If Mahi Manager Implementation allows for repeating answers, then the
  // callback function should be bound as a repeating callback. Else, a BindOnce
  // callback will be used.
  if (chromeos::MahiManager::Get()->AllowRepeatingAnswers()) {
    chromeos::MahiManager::Get()->AnswerQuestionRepeating(
        question, current_panel_content,
        base::BindRepeating(&MahiUiController::OnAnswerLoaded,
                            weak_ptr_factory_.GetWeakPtr()));
  } else {
    chromeos::MahiManager::Get()->AnswerQuestion(
        question, current_panel_content,
        base::BindOnce(&MahiUiController::OnAnswerLoaded,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void MahiUiController::UpdateSummaryAndOutlines() {
  InvalidatePendingRequests();

  chromeos::MahiManager::Get()->GetSummary(base::BindOnce(
      &MahiUiController::OnSummaryLoaded, weak_ptr_factory_.GetWeakPtr()));
  chromeos::MahiManager::Get()->GetOutlines(base::BindOnce(
      &MahiUiController::OnOutlinesLoaded, weak_ptr_factory_.GetWeakPtr()));
}

void MahiUiController::OnSessionStateChanged(
    session_manager::SessionState state) {
  if (state != session_manager::SessionState::ACTIVE) {
    RecordTimesPanelOpenedMetric();
    CloseMahiPanel();
  }
}

void MahiUiController::OnActiveUserSessionChanged(const AccountId& account_id) {
  CloseMahiPanel();
}

void MahiUiController::RecordTimesPanelOpenedMetric() {
  if (times_panel_opened_per_session_ > 0) {
    base::UmaHistogramCounts1000(
        mahi_constants::kTimesMahiPanelOpenedPerSessionHistogramName,
        times_panel_opened_per_session_);
  }

  times_panel_opened_per_session_ = 0;
}

void MahiUiController::HandleError(const MahiUiError& error) {
  // `chromeos::MahiResponseStatus::kLowQuota` is a warning not an error.
  CHECK_NE(error.status, chromeos::MahiResponseStatus::kLowQuota);

  // The presentation of any error during `State::kQuestionAndAnswer` should be
  // embedded into the Q&A view instead of a separate view.
  const MahiUiUpdate update(MahiUiUpdateType::kErrorReceived, error);
  if (error.origin_state == VisibilityState::kQuestionAndAnswer) {
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
    update_summary_after_answer_question_ = false;
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

  if (update_summary_after_answer_question_) {
    // TODO(b/345621992): Add test to verify this behavior.
    UpdateSummaryAndOutlines();
    update_summary_after_answer_question_ = false;
  }
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

void MahiUiController::InvalidatePendingRequests() {
  // By invalidating existing weak ptrs, the pending `OnAnswerLoaded`,
  // `OnOutlinesLoaded` and `OnSummaryLoaded` callbacks are cancelled.
  weak_ptr_factory_.InvalidateWeakPtrs();
}

}  // namespace ash
