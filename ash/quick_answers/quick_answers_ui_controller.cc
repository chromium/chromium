// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_answers/quick_answers_ui_controller.h"

#include "ash/components/quick_answers/quick_answers_model.h"
#include "ash/public/cpp/assistant/controller/assistant_interaction_controller.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/quick_answers/quick_answers_controller_impl.h"
#include "ash/quick_answers/ui/quick_answers_view.h"
#include "ash/quick_answers/ui/user_consent_view.h"
#include "ash/quick_answers/ui/user_notice_view.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "chromeos/services/assistant/public/cpp/assistant_service.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/escape.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

using quick_answers::QuickAnswer;
using quick_answers::QuickAnswersExitPoint;

constexpr char kGoogleSearchUrlPrefix[] = "https://www.google.com/search?q=";

constexpr char kFeedbackDescriptionTemplate[] = "#QuickAnswers\nQuery:%s\n";

}  // namespace

QuickAnswersUiController::QuickAnswersUiController(
    QuickAnswersControllerImpl* controller)
    : controller_(controller) {}

QuickAnswersUiController::~QuickAnswersUiController() {
  user_notice_view_ = nullptr;
}

void QuickAnswersUiController::CreateQuickAnswersView(const gfx::Rect& bounds,
                                                      const std::string& title,
                                                      const std::string& query,
                                                      bool is_internal) {
  // Currently there are timing issues that causes the quick answers view is not
  // dismissed. TODO(updowndota): Remove the special handling after the root
  // cause is found.
  if (IsShowingQuickAnswersView()) {
    LOG(ERROR) << "Quick answers view not dismissed.";
    CloseQuickAnswersView();
  }

  DCHECK(!user_notice_view_);
  DCHECK(!IsShowingUserConsentView());
  SetActiveQuery(query);

  // Owned by view hierarchy.
  auto* const quick_answers_view = new QuickAnswersView(
      bounds, title, is_internal, weak_factory_.GetWeakPtr());
  quick_answers_view_tracker_.SetView(quick_answers_view);
  quick_answers_view->GetWidget()->ShowInactive();
}

void QuickAnswersUiController::OnQuickAnswersViewPressed() {
  // Route dismissal through |controller_| for logging impressions.
  controller_->DismissQuickAnswers(QuickAnswersExitPoint::kQuickAnswersClick);

  NewWindowDelegate::GetInstance()->OpenUrl(
      GURL(kGoogleSearchUrlPrefix +
           net::EscapeUrlEncodedData(query_, /*use_plus=*/true)),
      /*from_user_interaction=*/true);
  controller_->OnQuickAnswerClick();
}

bool QuickAnswersUiController::CloseQuickAnswersView() {
  if (IsShowingQuickAnswersView()) {
    quick_answers_view()->GetWidget()->Close();
    return true;
  }
  return false;
}

void QuickAnswersUiController::OnRetryLabelPressed() {
  controller_->OnRetryQuickAnswersRequest();
}

void QuickAnswersUiController::RenderQuickAnswersViewWithResult(
    const gfx::Rect& anchor_bounds,
    const QuickAnswer& quick_answer) {
  if (!IsShowingQuickAnswersView())
    return;

  // QuickAnswersView was initiated with a loading page and will be updated
  // when quick answers result from server side is ready.
  quick_answers_view()->UpdateView(anchor_bounds, quick_answer);
}

void QuickAnswersUiController::SetActiveQuery(const std::string& query) {
  query_ = query;
}

void QuickAnswersUiController::ShowRetry() {
  if (!IsShowingQuickAnswersView())
    return;

  quick_answers_view()->ShowRetryView();
}

void QuickAnswersUiController::UpdateQuickAnswersBounds(
    const gfx::Rect& anchor_bounds) {
  if (IsShowingQuickAnswersView())
    quick_answers_view()->UpdateAnchorViewBounds(anchor_bounds);

  if (user_notice_view_)
    user_notice_view_->UpdateAnchorViewBounds(anchor_bounds);

  if (IsShowingUserConsentView())
    user_consent_view()->UpdateAnchorViewBounds(anchor_bounds);
}

void QuickAnswersUiController::CreateUserNoticeView(
    const gfx::Rect& anchor_bounds,
    const std::u16string& intent_type,
    const std::u16string& intent_text) {
  DCHECK(!user_notice_view_);
  user_notice_view_ = new quick_answers::UserNoticeView(
      anchor_bounds, intent_type, intent_text, this);
  user_notice_view_->GetWidget()->ShowInactive();
}

void QuickAnswersUiController::CloseUserNoticeView() {
  if (user_notice_view_) {
    user_notice_view_->GetWidget()->Close();
    user_notice_view_ = nullptr;
  }
}

void QuickAnswersUiController::CreateUserConsentView(
    const gfx::Rect& anchor_bounds,
    const std::u16string& intent_type,
    const std::u16string& intent_text) {
  DCHECK(!user_notice_view_);
  DCHECK(!IsShowingQuickAnswersView());
  DCHECK(!IsShowingUserConsentView());

  // Owned by view hierarchy.
  auto* const user_consent_view = new quick_answers::UserConsentView(
      anchor_bounds, intent_type, intent_text, weak_factory_.GetWeakPtr());
  user_consent_view_tracker_.SetView(user_consent_view);
  user_consent_view->GetWidget()->ShowInactive();
}

void QuickAnswersUiController::CloseUserConsentView() {
  if (IsShowingUserConsentView()) {
    user_consent_view()->GetWidget()->Close();
  }
}

void QuickAnswersUiController::OnAcceptButtonPressed() {
  DCHECK(user_notice_view_);
  controller_->OnUserNoticeAccepted();

  // The Quick-Answer displayed should gain focus if it is created when this
  // button is pressed.
  if (IsShowingQuickAnswersView())
    quick_answers_view()->RequestFocus();
}

void QuickAnswersUiController::OnManageSettingsButtonPressed() {
  controller_->OnNoticeSettingsRequestedByUser();
}

void QuickAnswersUiController::OnSettingsButtonPressed() {
  // Route dismissal through |controller_| for logging impressions.
  controller_->DismissQuickAnswers(QuickAnswersExitPoint::kSettingsButtonClick);

  controller_->OpenQuickAnswersSettings();
}

void QuickAnswersUiController::OnReportQueryButtonPressed() {
  controller_->DismissQuickAnswers(
      QuickAnswersExitPoint::kReportQueryButtonClick);

  NewWindowDelegate::GetInstance()->OpenFeedbackPage(
      NewWindowDelegate::FeedbackSource::kFeedbackSourceQuickAnswers,
      base::StringPrintf(kFeedbackDescriptionTemplate, query_.c_str()));
}

void QuickAnswersUiController::OnUserConsentResult(bool consented) {
  DCHECK(IsShowingUserConsentView());
  controller_->OnUserConsentResult(consented);

  if (consented && IsShowingQuickAnswersView())
    quick_answers_view()->RequestFocus();
}

bool QuickAnswersUiController::IsShowingUserConsentView() const {
  return user_consent_view_tracker_.view() &&
         !user_consent_view_tracker_.view()->GetWidget()->IsClosed();
}

bool QuickAnswersUiController::IsShowingQuickAnswersView() const {
  return quick_answers_view_tracker_.view() &&
         !quick_answers_view_tracker_.view()->GetWidget()->IsClosed();
}

}  // namespace ash
