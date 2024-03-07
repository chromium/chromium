// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/mahi/mahi_manager_impl.h"

#include <stdint.h>

#include <algorithm>
#include <vector>

#include "ash/system/mahi/mahi_panel_widget.h"
#include "base/functional/callback.h"
#include "base/strings/string_number_conversions.h"
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

}  // namespace

namespace ash {

MahiManagerImpl::MahiManagerImpl() = default;

MahiManagerImpl::~MahiManagerImpl() {
  mahi_panel_widget_.reset();
}

void MahiManagerImpl::OpenMahiPanel(int64_t display_id) {
  mahi_panel_widget_ = MahiPanelWidget::CreatePanelWidget(display_id);
  mahi_panel_widget_->Show();
}

std::u16string MahiManagerImpl::GetContentTitle() {
  return u"test content title";
}

gfx::ImageSkia MahiManagerImpl::GetContentIcon() {
  int test_size_px = 128;
  SkBitmap test_bitmap;
  test_bitmap.allocN32Pixels(test_size_px, test_size_px);
  test_bitmap.eraseColor(SK_ColorGREEN);
  return gfx::ImageSkia::CreateFrom1xBitmap(test_bitmap);
}

void MahiManagerImpl::GetSummary(MahiSummaryCallback callback) {
  auto* mahi_browser_delegate_ash = crosapi::CrosapiManager::Get()
                                        ->crosapi_ash()
                                        ->mahi_browser_delegate_ash();
  CHECK(mahi_browser_delegate_ash);

  if (!mahi_provider_) {
    mahi_provider_ = CreateProvider();
  }

  mahi_browser_delegate_ash->GetContentFromClient(
      current_page_info_->client_id, current_page_info_->page_id,
      base::BindOnce(&MahiManagerImpl::OnGetPageContent,
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

void MahiManagerImpl::AnswerQuestion(const std::string& question,
                                     MahiAnswerQuestionCallback callback) {
  std::move(callback).Run(u"test answer", MahiResponseStatus::kSuccess);
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

void MahiManagerImpl::OnGetPageContent(
    MahiSummaryCallback callback,
    crosapi::mojom::MahiPageContentPtr mahi_content_ptr) {
  if (!mahi_content_ptr) {
    std::move(callback).Run(u"summary text",
                            MahiResponseStatus::kContentExtractionError);
    return;
  }
  mahi_provider_->Summarize(
      base::UTF16ToUTF8(mahi_content_ptr->page_content),
      base::BindOnce(
          [](MahiSummaryCallback summary_callback, base::Value::Dict dict,
             manta::MantaStatus status) {
            if (status.status_code != manta::MantaStatusCode::kOk) {
              std::move(summary_callback)
                  .Run(u"Couldn't get summary",
                       MahiResponseStatus::kUnknownError);
              return;
            }

            if (auto* text = dict.FindString("outputData")) {
              std::move(summary_callback)
                  .Run(base::UTF8ToUTF16(*text), MahiResponseStatus::kSuccess);
            } else {
              std::move(summary_callback)
                  .Run(u"Cannot find outputdata",
                       MahiResponseStatus::kCantFindOutputData);
            }
          },
          std::move(callback)));
}

void MahiManagerImpl::OpenFeedbackDialog() {
  const std::string description_template = "#Mahi: ";

  base::Value::Dict ai_metadata;
  ai_metadata.Set("from_chromeos", "true");

  chrome::ShowFeedbackPage(
      /*browser=*/chrome::FindBrowserWithActiveWindow(),
      /*source=*/chrome::kFeedbackSourceAI, description_template,
      /*description_placeholder_text=*/
      base::UTF16ToUTF8(
          l10n_util::GetStringUTF16(IDS_SEA_PEN_FEEDBACK_PLACEHOLDER)),
      /*category_tag=*/std::string(),
      /*extra_diagnostics=*/std::string(),
      /*autofill_metadata=*/base::Value::Dict(), std::move(ai_metadata));
}

}  // namespace ash
