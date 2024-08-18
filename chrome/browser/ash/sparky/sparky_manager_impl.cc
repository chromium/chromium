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
#include "base/functional/callback.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/mahi/mahi_browser_delegate_ash.h"
#include "chrome/browser/ash/sparky/sparky_delegate_impl.h"
#include "chromeos/ash/components/sparky/system_info_delegate_impl.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"
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

ash::MahiBrowserDelegateAsh* GetMahiBrowserDelgateAsh() {
  auto* mahi_browser_delegate_ash = crosapi::CrosapiManager::Get()
                                        ->crosapi_ash()
                                        ->mahi_browser_delegate_ash();
  CHECK(mahi_browser_delegate_ash);
  return mahi_browser_delegate_ash;
}

}  // namespace
namespace ash {

SparkyManagerImpl::SparkyManagerImpl(Profile* profile,
                                     manta::MantaService* manta_service)
    : profile_(profile),
      sparky_provider_(manta_service->CreateSparkyProvider(
          std::make_unique<SparkyDelegateImpl>(profile),
          std::make_unique<sparky::SystemInfoDelegateImpl>())) {
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

void SparkyManagerImpl::GetSummary(MahiSummaryCallback callback) {
  GetMahiBrowserDelgateAsh()->GetContentFromClient(
      current_page_info_->client_id, current_page_info_->page_id,
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
    // Add the current question to the dialog.
    dialog_turns_.emplace_back(base::UTF16ToUTF8(question), manta::Role::kUser);

    auto sparky_context = std::make_unique<manta::SparkyContext>(
        dialog_turns_, base::UTF16ToUTF8(current_panel_content_->page_content));
    sparky_context->server_url = ash::switches::ObtainSparkyServerUrl();
    sparky_context->page_url = current_page_info_->url.spec();
    sparky_context->files = sparky_provider_->GetFilesSummary();

    sparky_provider_->QuestionAndAnswer(
        std::move(sparky_context),
        base::BindOnce(&SparkyManagerImpl::OnSparkyProviderQAResponse,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }

  GetMahiBrowserDelgateAsh()->GetContentFromClient(
      current_page_info_->client_id, current_page_info_->page_id,
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
      ui_controller_.OpenMahiPanel(
          context_menu_request->display_id,
          context_menu_request->mahi_menu_bounds.has_value()
              ? context_menu_request->mahi_menu_bounds.value()
              : gfx::Rect());
      return;
    case MahiContextMenuActionType::kQA:
      ui_controller_.OpenMahiPanel(
          context_menu_request->display_id,
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

void SparkyManagerImpl::OnSparkyProviderQAResponse(
    MahiAnswerQuestionCallbackRepeating callback,
    manta::MantaStatus status,
    manta::DialogTurn* latest_turn) {
  // Currently the history of dialogs will only refresh if the user closes the
  // UI and then reopens it again.
  // TODO (b/352651459): Add a refresh button to reset the dialog.

  if (status.status_code != manta::MantaStatusCode::kOk) {
    latest_response_status_ = MahiResponseStatus::kUnknownError;
    std::move(callback).Run(std::nullopt, latest_response_status_);
    return;
  }

  if (latest_turn) {
    latest_response_status_ = MahiResponseStatus::kSuccess;
    callback.Run(base::UTF8ToUTF16(latest_turn->message),
                 latest_response_status_);

    dialog_turns_.emplace_back(std::move(*latest_turn));

    auto sparky_context = std::make_unique<manta::SparkyContext>(
        dialog_turns_, base::UTF16ToUTF8(current_panel_content_->page_content));
    sparky_context->server_url = ash::switches::ObtainSparkyServerUrl();
    sparky_context->page_url = current_page_info_->url.spec();
    sparky_context->files = sparky_provider_->GetFilesSummary();
    CheckTurnLimit();

    // If the latest action is not the final action from the server, then an
    // additional request is made to the server. The last action must be of type
    // kAllDone to prevent an additional call.
    if (!latest_turn->actions.empty() &&
        (latest_turn->actions.back().type != manta::ActionType::kAllDone ||
         !latest_turn->actions.back().all_done)) {
      sparky_provider_->QuestionAndAnswer(
          std::move(sparky_context),
          base::BindOnce(&SparkyManagerImpl::OnSparkyProviderQAResponse,
                         weak_ptr_factory_.GetWeakPtr(), callback));
    }

  } else {
    latest_response_status_ = MahiResponseStatus::kCantFindOutputData;
    std::move(callback).Run(std::nullopt, latest_response_status_);
  }
}

void SparkyManagerImpl::CheckTurnLimit() {
  // If the size of the dialog does not exceed the turn limit then return.
  if (dialog_turns_.size() < kMaxConsecutiveTurns) {
    return;
  }
  // If the last action is already set to not made an additional server call
  // then return.
  if (dialog_turns_.back().actions.empty() ||
      dialog_turns_.back().actions.back().type != manta::ActionType::kAllDone ||
      dialog_turns_.back().actions.back().all_done == true) {
    return;
  }
  // Iterate through the last n turns if any of them are from the user then
  // return as the turn limit has not yet been reached.
  for (int position = 1; position < kMaxConsecutiveTurns; ++position) {
    auto turn = dialog_turns_.at(dialog_turns_.size() - kMaxConsecutiveTurns);
    if (turn.role == manta::Role::kUser) {
      return;
    }
  }
  // Assign the last action as all done to prevent any additional calls to the
  // server.
  dialog_turns_.back().actions.back().all_done = true;
}

void SparkyManagerImpl::OnGetPageContentForQA(
    const std::u16string& question,
    MahiAnswerQuestionCallbackRepeating callback,
    crosapi::mojom::MahiPageContentPtr mahi_content_ptr) {
  if (!mahi_content_ptr) {
    std::move(callback).Run(std::nullopt,
                            MahiResponseStatus::kContentExtractionError);
    return;
  }

  // Assign current panel content and clear the current panel QA
  current_panel_content_ = std::move(mahi_content_ptr);

  // Add the current question to the dialog.
  dialog_turns_.emplace_back(base::UTF16ToUTF8(question), manta::Role::kUser);

  auto sparky_context = std::make_unique<manta::SparkyContext>(
      dialog_turns_, base::UTF16ToUTF8(current_panel_content_->page_content));
  sparky_context->server_url = ash::switches::ObtainSparkyServerUrl();
  sparky_context->page_url = current_page_info_->url.spec();
  sparky_context->files = sparky_provider_->GetFilesSummary();

  sparky_provider_->QuestionAndAnswer(
      std::move(sparky_context),
      base::BindOnce(&SparkyManagerImpl::OnSparkyProviderQAResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
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

void SparkyManagerImpl::OpenFeedbackDialog() {}

}  // namespace ash
