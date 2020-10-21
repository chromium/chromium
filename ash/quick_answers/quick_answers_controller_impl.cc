// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_answers/quick_answers_controller_impl.h"

#include "ash/public/cpp/new_window_delegate.h"
#include "ash/quick_answers/quick_answers_ui_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "chromeos/components/quick_answers/quick_answers_consents.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {
using chromeos::quick_answers::Context;
using chromeos::quick_answers::IntentType;
using chromeos::quick_answers::QuickAnswer;
using chromeos::quick_answers::QuickAnswersClient;
using chromeos::quick_answers::QuickAnswersRequest;
using chromeos::quick_answers::ResultType;

constexpr char kAssistantRelatedInfoUrl[] =
    "chrome://os-settings/googleAssistant";
constexpr char kDogfoodUrl[] =
    "https://goto.google.com/quick-answers-dogfood-bugs";

// TODO:(yanxiao) move the string to grd source file.
constexpr char kNoResult[] = "See result in Assistant";

base::string16 IntentTypeToString(IntentType intent_type) {
  switch (intent_type) {
    case IntentType::kUnit:
      return l10n_util::GetStringUTF16(
          IDS_ASH_QUICK_ANSWERS_UNIT_CONVERSION_INTENT);
    case IntentType::kDictionary:
      return l10n_util::GetStringUTF16(IDS_ASH_QUICK_ANSWERS_DEFINITION_INTENT);
    case IntentType::kTranslation:
      return l10n_util::GetStringUTF16(
          IDS_ASH_QUICK_ANSWERS_TRANSLATION_INTENT);
    case IntentType::kUnknown:
      return base::string16();
  }
}

// Returns if the request has already been processed (by the text annotator).
bool IsProcessedRequest(const QuickAnswersRequest& request) {
  return (request.preprocessed_output.intent_info.intent_type !=
          chromeos::quick_answers::IntentType::kUnknown);
}

}  // namespace

namespace ash {

QuickAnswersControllerImpl::QuickAnswersControllerImpl()
    : quick_answers_ui_controller_(
          std::make_unique<ash::QuickAnswersUiController>(this)) {}

QuickAnswersControllerImpl::~QuickAnswersControllerImpl() = default;

void QuickAnswersControllerImpl::SetClient(
    std::unique_ptr<QuickAnswersClient> client) {
  quick_answers_client_ = std::move(client);
  consent_controller_ =
      std::make_unique<chromeos::quick_answers::QuickAnswersConsent>(
          Shell::Get()->session_controller()->GetPrimaryUserPrefService());
}

void QuickAnswersControllerImpl::MaybeShowQuickAnswers(
    const gfx::Rect& anchor_bounds,
    const std::string& title,
    const Context& context) {
  if (!is_eligible_)
    return;

  if (visibility_ == QuickAnswersVisibility::kClosed)
    return;

  // Cache anchor-bounds and query.
  anchor_bounds_ = anchor_bounds;
  // Initially, title is same as query. Title and query can be overridden based
  // on text annotation result at |OnRequestPreprocessFinish|.
  title_ = title;
  query_ = title;
  context_ = context;
  quick_answer_.reset();

  QuickAnswersRequest request = BuildRequest();
  if (chromeos::features::IsQuickAnswersTextAnnotatorEnabled()) {
    // Send the request for preprocessing. Only shows quick answers view if the
    // predicted intent is not |kUnknown| at |OnRequestPreprocessFinish|.
    quick_answers_client_->SendRequestForPreprocessing(request);
  } else {
    HandleQuickAnswerRequest(request);
  }
}

void QuickAnswersControllerImpl::HandleQuickAnswerRequest(
    const chromeos::quick_answers::QuickAnswersRequest& request) {
  if (ShouldShowUserConsent()) {
    ShowUserConsent(
        IntentTypeToString(request.preprocessed_output.intent_info.intent_type),
        base::UTF8ToUTF16(request.preprocessed_output.intent_info.intent_text));
  } else {
    visibility_ = QuickAnswersVisibility::kVisible;
    quick_answers_ui_controller_->CreateQuickAnswersView(anchor_bounds_, title_,
                                                         query_);

    if (IsProcessedRequest(request))
      quick_answers_client_->FetchQuickAnswers(request);
    else
      quick_answers_client_->SendRequest(request);
  }
}

void QuickAnswersControllerImpl::DismissQuickAnswers(bool is_active) {
  visibility_ = QuickAnswersVisibility::kClosed;
  MaybeDismissQuickAnswersConsent();
  bool closed = quick_answers_ui_controller_->CloseQuickAnswersView();
  quick_answers_client_->OnQuickAnswersDismissed(
      quick_answer_ ? quick_answer_->result_type : ResultType::kNoResult,
      is_active && closed);
}

chromeos::quick_answers::QuickAnswersDelegate*
QuickAnswersControllerImpl::GetQuickAnswersDelegate() {
  return this;
}

void QuickAnswersControllerImpl::OnQuickAnswerReceived(
    std::unique_ptr<QuickAnswer> quick_answer) {
  if (visibility_ != QuickAnswersVisibility::kVisible)
    return;

  if (quick_answer) {
    if (quick_answer->title.empty()) {
      quick_answer->title.push_back(
          std::make_unique<chromeos::quick_answers::QuickAnswerText>(title_));
    }
    quick_answers_ui_controller_->RenderQuickAnswersViewWithResult(
        anchor_bounds_, *quick_answer);
  } else {
    chromeos::quick_answers::QuickAnswer quick_answer_with_no_result;
    quick_answer_with_no_result.title.push_back(
        std::make_unique<chromeos::quick_answers::QuickAnswerText>(title_));
    quick_answer_with_no_result.first_answer_row.push_back(
        std::make_unique<chromeos::quick_answers::QuickAnswerResultText>(
            kNoResult));
    quick_answers_ui_controller_->RenderQuickAnswersViewWithResult(
        anchor_bounds_, quick_answer_with_no_result);
    // Fallback query to title if no result is available.
    query_ = title_;
    quick_answers_ui_controller_->SetActiveQuery(query_);
  }

  quick_answer_ = std::move(quick_answer);
}

void QuickAnswersControllerImpl::OnEligibilityChanged(bool eligible) {
  is_eligible_ = eligible;
}

void QuickAnswersControllerImpl::OnNetworkError() {
  if (visibility_ != QuickAnswersVisibility::kVisible)
    return;

  // Notify quick_answers_ui_controller_ to show retry UI.
  quick_answers_ui_controller_->ShowRetry();
}

void QuickAnswersControllerImpl::OnRequestPreprocessFinished(
    const QuickAnswersRequest& processed_request) {
  if (!chromeos::features::IsQuickAnswersTextAnnotatorEnabled()) {
    // Ignore preprocessing result if text annotator is not enabled.
    return;
  }

  auto intent_type =
      processed_request.preprocessed_output.intent_info.intent_type;

  if (intent_type == chromeos::quick_answers::IntentType::kUnknown) {
    return;
  }

  if (visibility_ == QuickAnswersVisibility::kClosed)
    return;

  query_ = processed_request.preprocessed_output.query;
  title_ = processed_request.preprocessed_output.intent_info.intent_text;

  HandleQuickAnswerRequest(processed_request);
}

void QuickAnswersControllerImpl::OnRetryQuickAnswersRequest() {
  QuickAnswersRequest request = BuildRequest();
  if (chromeos::features::IsQuickAnswersTextAnnotatorEnabled()) {
    quick_answers_client_->SendRequestForPreprocessing(request);
  } else {
    quick_answers_client_->SendRequest(request);
  }
}

void QuickAnswersControllerImpl::OnQuickAnswerClick() {
  quick_answers_client_->OnQuickAnswerClick(
      quick_answer_ ? quick_answer_->result_type : ResultType::kNoResult);
}

void QuickAnswersControllerImpl::UpdateQuickAnswersAnchorBounds(
    const gfx::Rect& anchor_bounds) {
  anchor_bounds_ = anchor_bounds;
  quick_answers_ui_controller_->UpdateQuickAnswersBounds(anchor_bounds);
}

void QuickAnswersControllerImpl::SetPendingShowQuickAnswers() {
  visibility_ = QuickAnswersVisibility::kPending;
}

void QuickAnswersControllerImpl::OnUserConsentGranted() {
  quick_answers_ui_controller_->CloseUserConsentView();
  consent_controller_->AcceptConsent(
      chromeos::quick_answers::ConsentInteractionType::kAccept);

  // Display Quick-Answer for the cached query when user consents.
  MaybeShowQuickAnswers(anchor_bounds_, title_, context_);
}

void QuickAnswersControllerImpl::OnConsentSettingsRequestedByUser() {
  quick_answers_ui_controller_->CloseUserConsentView();
  consent_controller_->AcceptConsent(
      chromeos::quick_answers::ConsentInteractionType::kManageSettings);
  NewWindowDelegate::GetInstance()->NewTabWithUrl(
      GURL(kAssistantRelatedInfoUrl), /*from_user_interaction=*/true);
}

void QuickAnswersControllerImpl::OpenQuickAnswersDogfoodLink() {
  NewWindowDelegate::GetInstance()->NewTabWithUrl(
      GURL(kDogfoodUrl), /*from_user_interaction=*/true);
}

void QuickAnswersControllerImpl::MaybeDismissQuickAnswersConsent() {
  if (quick_answers_ui_controller_->is_showing_user_consent_view())
    consent_controller_->DismissConsent();
  quick_answers_ui_controller_->CloseUserConsentView();
}

bool QuickAnswersControllerImpl::ShouldShowUserConsent() const {
  return consent_controller_->ShouldShowConsent();
}

void QuickAnswersControllerImpl::ShowUserConsent(
    const base::string16& intent_type,
    const base::string16& intent_text) {
  // Show user-consent notice informing user about the feature if required.
  if (!quick_answers_ui_controller_->is_showing_user_consent_view()) {
    quick_answers_ui_controller_->CreateUserConsentView(
        anchor_bounds_, intent_type, intent_text);
    consent_controller_->StartConsent();
  }
}

QuickAnswersRequest QuickAnswersControllerImpl::BuildRequest() {
  QuickAnswersRequest request;
  request.selected_text = title_;
  request.context = context_;
  return request;
}
}  // namespace ash
