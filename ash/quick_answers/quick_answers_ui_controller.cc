// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_answers/quick_answers_ui_controller.h"

#include "ash/public/cpp/assistant/controller/assistant_interaction_controller.h"
#include "ash/quick_answers/quick_answers_controller_impl.h"
#include "ash/quick_answers/ui/quick_answers_view.h"
#include "ash/quick_answers/ui/user_notice_view.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/bind.h"
#include "base/optional.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/services/assistant/public/cpp/assistant_service.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/widget/widget.h"

using chromeos::quick_answers::QuickAnswer;
namespace ash {

QuickAnswersUiController::QuickAnswersUiController(
    QuickAnswersControllerImpl* controller)
    : controller_(controller) {}

QuickAnswersUiController::~QuickAnswersUiController() {
  quick_answers_view_ = nullptr;
  user_notice_view_ = nullptr;
}

void QuickAnswersUiController::CreateQuickAnswersView(
    const gfx::Rect& bounds,
    const std::string& title,
    const std::string& query) {
  // Currently there are timing issues that causes the quick answers view is not
  // dismissed. TODO(updowndota): Remove the special handling after the root
  // cause is found.
  if (quick_answers_view_) {
    LOG(ERROR) << "Quick answers view not dismissed.";
    CloseQuickAnswersView();
  }

  DCHECK(!user_notice_view_);
  SetActiveQuery(query);
  quick_answers_view_ = new QuickAnswersView(bounds, title, this);
  quick_answers_view_->GetWidget()->ShowInactive();
}

void QuickAnswersUiController::OnQuickAnswersViewPressed() {
  // Route dismissal through |controller_| for logging impressions.
  controller_->DismissQuickAnswers(/*is_active=*/true);

  ash::AssistantInteractionController::Get()->StartTextInteraction(
      query_, /*allow_tts=*/false,
      chromeos::assistant::AssistantQuerySource::kQuickAnswers);
  controller_->OnQuickAnswerClick();
}

bool QuickAnswersUiController::CloseQuickAnswersView() {
  if (quick_answers_view_) {
    quick_answers_view_->GetWidget()->Close();
    quick_answers_view_ = nullptr;
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
  if (!quick_answers_view_)
    return;

  // QuickAnswersView was initiated with a loading page and will be updated
  // when quick answers result from server side is ready.
  quick_answers_view_->UpdateView(anchor_bounds, quick_answer);
}

void QuickAnswersUiController::SetActiveQuery(const std::string& query) {
  query_ = query;
}

void QuickAnswersUiController::ShowRetry() {
  if (!quick_answers_view_)
    return;

  quick_answers_view_->ShowRetryView();
}

void QuickAnswersUiController::UpdateQuickAnswersBounds(
    const gfx::Rect& anchor_bounds) {
  if (quick_answers_view_)
    quick_answers_view_->UpdateAnchorViewBounds(anchor_bounds);

  if (user_notice_view_)
    user_notice_view_->UpdateAnchorViewBounds(anchor_bounds);
}

void QuickAnswersUiController::CreateUserNoticeView(
    const gfx::Rect& anchor_bounds,
    const base::string16& intent_type,
    const base::string16& intent_text) {
  DCHECK(!quick_answers_view_);
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

void QuickAnswersUiController::OnAcceptButtonPressed() {
  DCHECK(user_notice_view_);
  controller_->OnUserNoticeAccepted();

  // The Quick-Answer displayed should gain focus if it is created when this
  // button is pressed.
  if (quick_answers_view_)
    quick_answers_view_->RequestFocus();
}

void QuickAnswersUiController::OnManageSettingsButtonPressed() {
  controller_->OnNoticeSettingsRequestedByUser();
}

void QuickAnswersUiController::OnDogfoodButtonPressed() {
  // Route dismissal through |controller_| for logging impressions.
  controller_->DismissQuickAnswers(/*is_active=*/true);

  controller_->OpenQuickAnswersDogfoodLink();
}

}  // namespace ash
