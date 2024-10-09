// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/sparky/sparky_manager_impl.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/mahi/mahi_panel_widget.h"
#include "ash/system/mahi/mahi_ui_controller.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/sparky/sparky_delegate_impl.h"
#include "chromeos/ash/components/sparky/system_info_delegate_impl.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"
#include "chromeos/components/mahi/public/cpp/mahi_web_contents_manager.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/manta/features.h"
#include "components/manta/manta_service.h"
#include "components/manta/proto/sparky.pb.h"
#include "components/manta/sparky/sparky_context.h"
#include "components/manta/sparky/sparky_util.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace {

using chromeos::MahiResponseStatus;
using crosapi::mojom::MahiContextMenuActionType;
constexpr int kMaxConsecutiveTurns = 20;
constexpr base::TimeDelta kWaitBeforeAdditionalCall = base::Seconds(2);

}  // namespace
namespace ash {

namespace {

std::u16string GenerateErrorMessage(const std::string& existing_message,
                                    manta::MantaStatusCode status_code) {
  std::u16string message = base::UTF8ToUTF16(existing_message);
  message += u"\nManta Error Status:";
  switch (status_code) {
    case manta::MantaStatusCode::kOk:
      message += u"OK";
      break;
    case manta::MantaStatusCode::kGenericError:
      message += u"Generic Error";
      break;
    case manta::MantaStatusCode::kInvalidInput:
      message += u"Invalid Input";
      break;
    case manta::MantaStatusCode::kResourceExhausted:
      message += u"Resource Exhausted";
      break;
    case manta::MantaStatusCode::kBackendFailure:
      message += u"Backend Failure";
      break;
    case manta::MantaStatusCode::kMalformedResponse:
      message += u"Malformed Response";
      break;
    case manta::MantaStatusCode::kNoInternetConnection:
      message += u"No Internet Connection";
      break;
    case manta::MantaStatusCode::kUnsupportedLanguage:
      message += u"Unsupported Language";
      break;
    case manta::MantaStatusCode::kBlockedOutputs:
      message += u"Blocked Outputs";
      break;
    case manta::MantaStatusCode::kRestrictedCountry:
      message += u"Restricted Country";
      break;
    case manta::MantaStatusCode::kNoIdentityManager:
      message += u"No Identity Manager";
      break;
    case manta::MantaStatusCode::kPerUserQuotaExceeded:
      message += u"Per-User Quota Exceeded";
      break;
  }
  return message;
}
}  // namespace

SparkyManagerImpl::SparkyManagerImpl(Profile* profile,
                                     manta::MantaService* manta_service)
    : profile_(profile),
      sparky_provider_(manta_service->CreateSparkyProvider(
          std::make_unique<SparkyDelegateImpl>(profile),
          std::make_unique<sparky::SystemInfoDelegateImpl>())),
      timer_(std::make_unique<base::OneShotTimer>()) {
  CHECK(manta::features::IsMantaServiceEnabled());
}

SparkyManagerImpl::~SparkyManagerImpl() = default;

std::u16string SparkyManagerImpl::GetContentTitle() {
  return u"";
}

gfx::ImageSkia SparkyManagerImpl::GetContentIcon() {
  return gfx::ImageSkia();
}

GURL SparkyManagerImpl::GetContentUrl() {
  return current_page_info_->url;
}

void SparkyManagerImpl::GetContent(MahiContentCallback callback) {}

void SparkyManagerImpl::GetSummary(MahiSummaryCallback callback) {
  chromeos::MahiWebContentsManager::Get()->RequestContent(
      current_page_info_->page_id,
      base::BindOnce(&SparkyManagerImpl::OnGetPageContentForSummary,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void SparkyManagerImpl::GetOutlines(MahiOutlinesCallback callback) {
  std::vector<chromeos::MahiOutline> outlines;
  std::move(callback).Run(outlines, MahiResponseStatus::kUnknownError);
}

void SparkyManagerImpl::GoToOutlineContent(int outline_id) {}

void SparkyManagerImpl::AnswerQuestionRepeating(
    const std::u16string& question,
    bool current_panel_content,
    MahiAnswerQuestionCallbackRepeating callback) {
  if (current_panel_content) {
    // Creates a new turn for the current question.
    manta::proto::Turn new_turn = manta::CreateTurn(
        base::UTF16ToUTF8(question), manta::proto::Role::ROLE_USER);

    auto sparky_context = std::make_unique<manta::SparkyContext>(
        new_turn, base::UTF16ToUTF8(current_panel_content_->page_content));
    sparky_context->server_url = ash::switches::ObtainSparkyServerUrl();
    sparky_context->page_url = current_page_info_->url.spec();
    sparky_context->files = sparky_provider_->GetFilesSummary();

    RequestProviderWithQuestion(std::move(sparky_context), std::move(callback));
    return;
  }

  chromeos::MahiWebContentsManager::Get()->RequestContent(
      current_page_info_->page_id,
      base::BindOnce(&SparkyManagerImpl::OnGetPageContentForQA,
                     weak_ptr_factory_.GetWeakPtr(), question,
                     std::move(callback)));
}

void SparkyManagerImpl::GetSuggestedQuestion(
    MahiGetSuggestedQuestionCallback callback) {}

void SparkyManagerImpl::SetCurrentFocusedPageInfo(
    crosapi::mojom::MahiPageInfoPtr info) {
  GURL url_before_update = current_page_info_->url;
  current_page_info_ = std::move(info);
  bool did_url_change =
      !url_before_update.EqualsIgnoringRef(current_page_info_->url);

  bool available =
      current_page_info_->IsDistillable.value_or(false) && did_url_change;
  NotifyRefreshAvailability(/*available=*/available);
}

void SparkyManagerImpl::OnContextMenuClicked(
    crosapi::mojom::MahiContextMenuRequestPtr context_menu_request) {
  switch (context_menu_request->action_type) {
    case MahiContextMenuActionType::kSummary:
    case MahiContextMenuActionType::kOutline:
      // TODO(b/318565610): Update the behaviour of kOutline.
      OpenMahiPanel(context_menu_request->display_id,
                    context_menu_request->mahi_menu_bounds.has_value()
                        ? context_menu_request->mahi_menu_bounds.value()
                        : gfx::Rect());
      return;
    case MahiContextMenuActionType::kQA:
      OpenMahiPanel(context_menu_request->display_id,
                    context_menu_request->mahi_menu_bounds.has_value()
                        ? context_menu_request->mahi_menu_bounds.value()
                        : gfx::Rect());

      // Ask question.
      if (!context_menu_request->question) {
        return;
      }

      // When the user sends a question from the context menu, we treat it as
      // the start of a new journey, so we set `current_panel_content` false.
      ui_controller_.SendQuestion(context_menu_request->question.value(),
                                  /*current_panel_content=*/false,
                                  MahiUiController::QuestionSource::kMenuView);
      return;
    case MahiContextMenuActionType::kSettings:
      // TODO(b/318565610): Update the behaviour of kSettings
      return;
    case MahiContextMenuActionType::kNone:
      return;
  }
}

void SparkyManagerImpl::OpenFeedbackDialog() {}

void SparkyManagerImpl::OpenMahiPanel(int64_t display_id,
                                      const gfx::Rect& mahi_menu_bounds) {
  // When receiving a new open panel request, we treat it as a new session and
  // clear the previous conversations.
  // TODO(b:365674359) Ideally, we should clear dialog on sparky panel close
  // instead sparky panel open.
  sparky_provider_->ClearDialog();

  ui_controller_.OpenMahiPanel(display_id, mahi_menu_bounds);
}

bool SparkyManagerImpl::IsEnabled() {
  // TODO (b/333479467): Update with new pref for this feature.
  return chromeos::features::IsSparkyEnabled() &&
         ash::switches::IsSparkySecretKeyMatched() &&
         Shell::Get()->session_controller()->GetActivePrefService()->GetBoolean(
             ash::prefs::kHmrEnabled);
}

void SparkyManagerImpl::SetMediaAppPDFFocused() {}

void SparkyManagerImpl::NotifyRefreshAvailability(bool available) {
  if (ui_controller_.IsMahiPanelOpen()) {
    ui_controller_.NotifyRefreshAvailabilityChanged(available);
  }
}

void SparkyManagerImpl::OnGetPageContentForSummary(
    MahiSummaryCallback callback,
    crosapi::mojom::MahiPageContentPtr mahi_content_ptr) {
  if (!mahi_content_ptr) {
    std::move(callback).Run(u"summary text",
                            MahiResponseStatus::kContentExtractionError);
    return;
  }

  // Assign current panel content and clear the current panel QA
  current_panel_content_ = std::move(mahi_content_ptr);

  latest_response_status_ = MahiResponseStatus::kUnknownError;
  std::move(callback).Run(u"Couldn't get summary", latest_response_status_);
  return;
}

void SparkyManagerImpl::RequestProviderWithQuestion(
    std::unique_ptr<manta::SparkyContext> sparky_context,
    MahiAnswerQuestionCallbackRepeating callback) {
  sparky_provider_->QuestionAndAnswer(
      std::move(sparky_context),
      base::BindOnce(&SparkyManagerImpl::OnSparkyProviderQAResponse,
                     weak_ptr_factory_.GetWeakPtr(), callback));
}

void SparkyManagerImpl::OnSparkyProviderQAResponse(
    MahiAnswerQuestionCallbackRepeating callback,
    manta::MantaStatus status,
    manta::proto::Turn* latest_turn) {
  // Currently the history of dialogs will only refresh if the user closes the
  // UI and then reopens it again.
  // TODO (b/352651459): Add a refresh button to reset the dialog.

  if (status.status_code != manta::MantaStatusCode::kOk && latest_turn) {
    latest_response_status_ = MahiResponseStatus::kUnknownError;
    // Instead of relying the mahi's `MahiUiController::HandleError()`, displays
    // customized message in the dialog.
    std::move(callback).Run(
        GenerateErrorMessage(latest_turn->message(), status.status_code),
        MahiResponseStatus::kSuccess);
    return;
  }

  if (latest_turn) {
    latest_response_status_ = MahiResponseStatus::kSuccess;
    callback.Run(base::UTF8ToUTF16(latest_turn->message()),
                 latest_response_status_);

    auto sparky_context = std::make_unique<manta::SparkyContext>(
        *latest_turn, base::UTF16ToUTF8(current_panel_content_->page_content));
    sparky_context->server_url = ash::switches::ObtainSparkyServerUrl();
    sparky_context->page_url = current_page_info_->url.spec();
    sparky_context->files = sparky_provider_->GetFilesSummary();
    CheckTurnLimit();

    // If additional call is expected, then an additional request is made to the
    // server.
    if (sparky_provider_->is_additional_call_expected()) {
      timer_->Start(
          FROM_HERE, kWaitBeforeAdditionalCall,
          base::BindOnce(&SparkyManagerImpl::RequestProviderWithQuestion,
                         weak_ptr_factory_.GetWeakPtr(),
                         std::move(sparky_context), callback));
    }

  } else {
    latest_response_status_ = MahiResponseStatus::kCantFindOutputData;
    // Instead of relying the mahi's `MahiUiController::HandleError()`, displays
    // customized message in the dialog.
    std::move(callback).Run(
        GenerateErrorMessage("Sparky Manager Error: can't find output data.",
                             status.status_code),
        MahiResponseStatus::kSuccess);
  }
}

void SparkyManagerImpl::CheckTurnLimit() {
  // If the size of consecutive assistant turns at the end does not exceed the
  // turn limit then return.
  if (sparky_provider_->consecutive_assistant_turn_count() <
      kMaxConsecutiveTurns) {
    return;
  }
  // If additional call is already unexpected then return.
  if (!sparky_provider_->is_additional_call_expected()) {
    return;
  }
  // Assign the last action as all done to prevent any additional calls to the
  // server.
  sparky_provider_->MarkLastActionAllDone();
}

void SparkyManagerImpl::OnGetPageContentForQA(
    const std::u16string& question,
    MahiAnswerQuestionCallbackRepeating callback,
    crosapi::mojom::MahiPageContentPtr mahi_content_ptr) {
  if (!mahi_content_ptr) {
    // Instead of relying the mahi's `MahiUiController::HandleError()`, displays
    // customized message in the dialog.
    std::move(callback).Run(u"Sparky Manager Error: content extraction error.",
                            MahiResponseStatus::kSuccess);
    return;
  }

  // Assign current panel content and clear the current panel QA
  current_panel_content_ = std::move(mahi_content_ptr);

  // Creates a new turn for the current question.
  manta::proto::Turn new_turn = manta::CreateTurn(
      base::UTF16ToUTF8(question), manta::proto::Role::ROLE_USER);

  auto sparky_context = std::make_unique<manta::SparkyContext>(
      new_turn, base::UTF16ToUTF8(current_panel_content_->page_content));
  sparky_context->server_url = ash::switches::ObtainSparkyServerUrl();
  sparky_context->page_url = current_page_info_->url.spec();
  sparky_context->files = sparky_provider_->GetFilesSummary();

  RequestProviderWithQuestion(std::move(sparky_context), std::move(callback));
}

// This function will never be called as Sparky uses a repeating callback to
// respond to the question rather than a once callback.
void SparkyManagerImpl::AnswerQuestion(const std::u16string& question,
                                       bool current_panel_content,
                                       MahiAnswerQuestionCallback callback) {}

// Sparky allows for multi consecutive responses back from the server to
// complete the task requested by the user.
bool SparkyManagerImpl::AllowRepeatingAnswers() {
  return true;
}

}  // namespace ash
