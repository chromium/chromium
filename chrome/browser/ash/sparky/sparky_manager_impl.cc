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
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/manta/features.h"
#include "components/manta/manta_service.h"
#include "components/manta/proto/sparky.pb.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace {

using chromeos::MahiResponseStatus;
using crosapi::mojom::MahiContextMenuActionType;

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
          std::make_unique<SparkyDelegateImpl>(profile))) {
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

void SparkyManagerImpl::AnswerQuestion(const std::u16string& question,
                                       bool current_panel_content,
                                       MahiAnswerQuestionCallback callback) {
  if (current_panel_content) {
    sparky_provider_->QuestionAndAnswer(
        base::UTF16ToUTF8(current_panel_content_->page_content),
        current_panel_qa_, base::UTF16ToUTF8(question),
        manta::proto::Task::TASK_PLANNER,
        base::BindOnce(&SparkyManagerImpl::OnSparkyProviderQAResponse,
                       weak_ptr_factory_.GetWeakPtr(), question,
                       std::move(callback)));
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
      ui_controller_.OpenMahiPanel(context_menu_request->display_id);
      return;
    case MahiContextMenuActionType::kQA:
      ui_controller_.OpenMahiPanel(context_menu_request->display_id);

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
  return IsSupportedWithCorrectFeatureKey() &&
         Shell::Get()->session_controller()->GetActivePrefService()->GetBoolean(
             ash::prefs::kMahiEnabled);
}

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
  current_panel_qa_.clear();

  latest_response_status_ = MahiResponseStatus::kUnknownError;
  std::move(callback).Run(u"Couldn't get summary", latest_response_status_);
  return;
}

void SparkyManagerImpl::OnSparkyProviderQAResponse(
    const std::u16string& question,
    MahiAnswerQuestionCallback callback,
    const std::string& response,
    manta::MantaStatus status) {
  if (status.status_code != manta::MantaStatusCode::kOk) {
    latest_response_status_ = MahiResponseStatus::kUnknownError;
    current_panel_qa_.emplace_back(base::UTF16ToUTF8(question), "");
    std::move(callback).Run(std::nullopt, latest_response_status_);
    return;
  }

  if (!response.empty()) {
    latest_response_status_ = MahiResponseStatus::kSuccess;
    current_panel_qa_.emplace_back(base::UTF16ToUTF8(question), response);
    std::move(callback).Run(base::UTF8ToUTF16(response),
                            latest_response_status_);
  } else {
    latest_response_status_ = MahiResponseStatus::kCantFindOutputData;
    std::move(callback).Run(std::nullopt, latest_response_status_);
  }
}

void SparkyManagerImpl::OnGetPageContentForQA(
    const std::u16string& question,
    MahiAnswerQuestionCallback callback,
    crosapi::mojom::MahiPageContentPtr mahi_content_ptr) {
  if (!mahi_content_ptr) {
    std::move(callback).Run(std::nullopt,
                            MahiResponseStatus::kContentExtractionError);
    return;
  }

  // Assign current panel content and clear the current panel QA
  current_panel_content_ = std::move(mahi_content_ptr);
  current_panel_qa_.clear();

  sparky_provider_->QuestionAndAnswer(
      base::UTF16ToUTF8(current_panel_content_->page_content),
      current_panel_qa_, base::UTF16ToUTF8(question),
      manta::proto::Task::TASK_PLANNER,
      base::BindOnce(&SparkyManagerImpl::OnSparkyProviderQAResponse,
                     weak_ptr_factory_.GetWeakPtr(), question,
                     std::move(callback)));
}

void SparkyManagerImpl::OpenFeedbackDialog() {}

}  // namespace ash
