// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/mahi/mahi_manager_impl.h"

#include <stdint.h>

#include <algorithm>
#include <string>
#include <vector>

#include "ash/system/mahi/mahi_panel_widget.h"
#include "base/functional/callback.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/mahi/mahi_browser_delegate_ash.h"
#include "chrome/browser/manta/manta_service_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/manta/features.h"
#include "components/manta/manta_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace {

using chromeos::MahiResponseStatus;
using crosapi::mojom::MahiContextMenuActionType;

std::unique_ptr<manta::MahiProvider> CreateProvider() {
  if (!manta::features::IsMantaServiceEnabled()) {
    return nullptr;
  }

  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (!profile) {
    return nullptr;
  }

  if (manta::MantaService* service =
          manta::MantaServiceFactory::GetForProfile(profile)) {
    return service->CreateMahiProvider();
  }

  return nullptr;
}

ash::MahiBrowserDelegateAsh* GetMahiBrowserDelgateAsh() {
  auto* mahi_browser_delegate_ash = crosapi::CrosapiManager::Get()
                                        ->crosapi_ash()
                                        ->mahi_browser_delegate_ash();
  CHECK(mahi_browser_delegate_ash);
  return mahi_browser_delegate_ash;
}

}  // namespace

namespace ash {

MahiManagerImpl::MahiManagerImpl() = default;

MahiManagerImpl::~MahiManagerImpl() {
  mahi_panel_widget_.reset();
  mahi_provider_.reset();
}

void MahiManagerImpl::OpenMahiPanel(int64_t display_id) {
  if (!IsEnabledWithCorrectFeatureKey()) {
    return;
  }

  mahi_panel_widget_ = MahiPanelWidget::CreatePanelWidget(display_id);
  mahi_panel_widget_->Show();
}

std::u16string MahiManagerImpl::GetContentTitle() {
  return current_page_info_->title;
}

gfx::ImageSkia MahiManagerImpl::GetContentIcon() {
  return current_page_info_->favicon_image;
}

void MahiManagerImpl::GetSummary(MahiSummaryCallback callback) {
  MaybeInitialize();
  GetMahiBrowserDelgateAsh()->GetContentFromClient(
      current_page_info_->client_id, current_page_info_->page_id,
      base::BindOnce(&MahiManagerImpl::OnGetPageContentForSummary,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void MahiManagerImpl::GetOutlines(MahiOutlinesCallback callback) {
  std::vector<chromeos::MahiOutline> outlines;
  for (int i = 0; i < 5; i++) {
    outlines.emplace_back(
        chromeos::MahiOutline(i, u"Outline " + base::NumberToString16(i)));
  }
  std::move(callback).Run(outlines, MahiResponseStatus::kSuccess);
}

void MahiManagerImpl::GoToOutlineContent(int outline_id) {}

void MahiManagerImpl::AnswerQuestion(const std::u16string& question,
                                     bool current_panel_content,
                                     MahiAnswerQuestionCallback callback) {
  MaybeInitialize();

  const std::u16string test_answer(u"test answer");

  if (current_panel_content) {
    std::move(callback).Run(test_answer, MahiResponseStatus::kSuccess);
    current_panel_qa_.emplace_back(question, test_answer);
    return;
  }

  current_panel_qa_.clear();
  GetMahiBrowserDelgateAsh()->GetContentFromClient(
      current_page_info_->client_id, current_page_info_->page_id,
      base::BindOnce(&MahiManagerImpl::OnGetPageContentForQA,
                     weak_ptr_factory_.GetWeakPtr(), question,
                     std::move(callback)));
}

void MahiManagerImpl::GetSuggestedQuestion(
    MahiGetSuggestedQuestionCallback callback) {
  std::move(callback).Run(u"test suggested question",
                          MahiResponseStatus::kSuccess);
}

void MahiManagerImpl::SetCurrentFocusedPageInfo(
    crosapi::mojom::MahiPageInfoPtr info) {
  // TODO(b/318565610): consider adding default icon when there is no icon
  // available.
  current_page_info_ = std::move(info);
}

void MahiManagerImpl::OnContextMenuClicked(
    crosapi::mojom::MahiContextMenuRequestPtr context_menu_request) {
  switch (context_menu_request->action_type) {
    case MahiContextMenuActionType::kSummary:
    case MahiContextMenuActionType::kOutline:
    case MahiContextMenuActionType::kQA:
      // TODO(b/318565610): Update the behaviour of kOutline and kQA
      OpenMahiPanel(context_menu_request->display_id);
      return;
    case MahiContextMenuActionType::kSettings:
      // TODO(b/318565610): Update the behaviour of kSettings
      return;
    case MahiContextMenuActionType::kNone:
      return;
  }
}

void MahiManagerImpl::NotifyRefreshAvailability(bool available) {
  auto* mahi_widget = static_cast<MahiPanelWidget*>(mahi_panel_widget_.get());
  if (mahi_widget) {
    mahi_widget->SetRefreshViewVisible(/*visible=*/available);
  }
}

void MahiManagerImpl::MaybeInitialize() {
  if (!mahi_provider_) {
    mahi_provider_ = CreateProvider();
  }
  CHECK(mahi_provider_);
}

void MahiManagerImpl::OnGetPageContentForSummary(
    MahiSummaryCallback callback,
    crosapi::mojom::MahiPageContentPtr mahi_content_ptr) {
  if (!mahi_content_ptr) {
    std::move(callback).Run(u"summary text",
                            MahiResponseStatus::kContentExtractionError);
    return;
  }

  current_panel_content_ = std::move(mahi_content_ptr);

  CHECK(mahi_provider_);
  mahi_provider_->Summarize(
      base::UTF16ToUTF8(current_panel_content_->page_content),
      base::BindOnce(&MahiManagerImpl::OnMahiProviderResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void MahiManagerImpl::OnMahiProviderResponse(
    MahiSummaryCallback summary_callback,
    base::Value::Dict dict,
    manta::MantaStatus status) {
  latest_summary_ = u"...";
  if (status.status_code != manta::MantaStatusCode::kOk) {
    latest_response_status_ = MahiResponseStatus::kUnknownError;
    std::move(summary_callback)
        .Run(u"Couldn't get summary", latest_response_status_);
    return;
  }

  if (auto* text = dict.FindString("outputData")) {
    latest_response_status_ = MahiResponseStatus::kSuccess;
    latest_summary_ = base::UTF8ToUTF16(*text);
    std::move(summary_callback).Run(latest_summary_, latest_response_status_);
  } else {
    latest_response_status_ = MahiResponseStatus::kCantFindOutputData;
    std::move(summary_callback)
        .Run(u"Cannot find outputdata", latest_response_status_);
  }
}

void MahiManagerImpl::OnGetPageContentForQA(
    const std::u16string& question,
    MahiAnswerQuestionCallback callback,
    crosapi::mojom::MahiPageContentPtr mahi_content_ptr) {
  const std::u16string test_answer(u"test answer");
  if (!mahi_content_ptr) {
    std::move(callback).Run(test_answer,
                            MahiResponseStatus::kContentExtractionError);
    return;
  }

  current_panel_content_ = std::move(mahi_content_ptr);

  std::move(callback).Run(test_answer, MahiResponseStatus::kSuccess);
  current_panel_qa_.emplace_back(question, test_answer);
}

void MahiManagerImpl::OpenFeedbackDialog() {
  const std::string description_template = base::StringPrintf(
      "#Mahi\nlatest status code: %d\nlatest summary: %s\nuser feedback:",
      static_cast<int>(latest_response_status_),
      base::UTF16ToUTF8(latest_summary_).c_str());

  base::Value::Dict ai_metadata;
  ai_metadata.Set("from_mahi", "true");

  chrome::ShowFeedbackPage(
      /*browser=*/chrome::FindBrowserWithActiveWindow(),
      /*source=*/chrome::kFeedbackSourceAI, description_template,
      /*description_placeholder_text=*/
      base::UTF16ToUTF8(
          l10n_util::GetStringUTF16(IDS_SEA_PEN_FEEDBACK_PLACEHOLDER)),
      /*category_tag=*/"mahi",
      /*extra_diagnostics=*/std::string(),
      /*autofill_metadata=*/base::Value::Dict(), std::move(ai_metadata));
}

}  // namespace ash
