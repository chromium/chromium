// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/quick_answers_menu_observer.h"

#include <utility>

#include "ash/public/cpp/assistant/controller/assistant_interaction_controller.h"
#include "ash/public/cpp/quick_answers/controller/quick_answers_controller.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/branding_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/components/quick_answers/utils/quick_answers_metrics.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/services/assistant/public/cpp/assistant_service.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/renderer_context_menu/render_view_context_menu_proxy.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/gfx/text_constants.h"
#include "ui/gfx/text_elider.h"

namespace {

using chromeos::quick_answers::Context;
using chromeos::quick_answers::QuickAnswer;
using chromeos::quick_answers::QuickAnswersClient;
using chromeos::quick_answers::QuickAnswersRequest;
using chromeos::quick_answers::ResultType;

// TODO(llin): Update the placeholder after finalizing on the design.
constexpr char kLoadingPlaceholder[] = "Loading...";
constexpr char kNoResult[] = "See result in Assistant";
constexpr char kNetworkError[] = "Cannot connect to internet.";

constexpr size_t kMaxDisplayTextLength = 70;
constexpr int kMaxSurroundingTextLength = 300;

base::string16 TruncateString(const std::string& text) {
  return gfx::TruncateString(base::UTF8ToUTF16(text), kMaxDisplayTextLength,
                             gfx::WORD_BREAK);
}

base::string16 SanitizeText(const base::string16& text) {
  base::string16 updated_text;
  // Escape Ampersands.
  base::ReplaceChars(text, base::ASCIIToUTF16("&"), base::ASCIIToUTF16("&&"),
                     &updated_text);

  // Remove invalid chars.
  base::ReplaceChars(updated_text, base::kWhitespaceUTF16,
                     base::ASCIIToUTF16(" "), &updated_text);

  return updated_text;
}

}  // namespace

QuickAnswersMenuObserver::QuickAnswersMenuObserver(
    RenderViewContextMenuProxy* proxy)
    : proxy_(proxy) {
  auto* assistant_state = ash::AssistantState::Get();
  if (assistant_state && proxy_ && proxy_->GetBrowserContext()) {
    auto* browser_context = proxy_->GetBrowserContext();
    if (browser_context->IsOffTheRecord())
      return;

    quick_answers_client_ = std::make_unique<QuickAnswersClient>(
        content::BrowserContext::GetDefaultStoragePartition(browser_context)
            ->GetURLLoaderFactoryForBrowserProcess()
            .get(),
        assistant_state, /*delegate=*/this);
    quick_answers_controller_ = ash::QuickAnswersController::Get();
    if (!quick_answers_controller_)
      return;
    quick_answers_controller_->SetClient(std::make_unique<QuickAnswersClient>(
        content::BrowserContext::GetDefaultStoragePartition(browser_context)
            ->GetURLLoaderFactoryForBrowserProcess()
            .get(),
        assistant_state, quick_answers_controller_->GetQuickAnswersDelegate()));
  }
}

QuickAnswersMenuObserver::~QuickAnswersMenuObserver() = default;

void QuickAnswersMenuObserver::InitMenu(
    const content::ContextMenuParams& params) {
  if (IsRichUiEnabled())
    return;

  if (!is_eligible_ || !proxy_ || !quick_answers_client_)
    return;

  // Skip password input field.
  if (params.input_field_type ==
      blink::ContextMenuDataInputFieldType::kPassword) {
    return;
  }

  // Skip editable text selection if the feature is not enabled.
  if (params.is_editable &&
      !chromeos::features::IsQuickAnswersOnEditableTextEnabled()) {
    return;
  }

  // Skip if no text selected.
  auto selected_text = base::UTF16ToUTF8(SanitizeText(params.selection_text));
  if (selected_text.empty())
    return;

  // Add Quick Answer Menu item.
  // TODO(llin): Update the menu item after finalizing on the design.
  auto truncated_text = TruncateString(selected_text);
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  proxy_->AddMenuItemWithIcon(
      IDC_CONTENT_CONTEXT_QUICK_ANSWERS_INLINE_QUERY, truncated_text,
      ui::ImageModel::FromVectorIcon(kAssistantIcon, /*color_id=*/-1,
                                     ui::SimpleMenuModel::kDefaultIconSize));
#else
  proxy_->AddMenuItem(IDC_CONTENT_CONTEXT_QUICK_ANSWERS_INLINE_QUERY,
                      truncated_text);
#endif
  proxy_->AddMenuItem(IDC_CONTENT_CONTEXT_QUICK_ANSWERS_INLINE_ANSWER,
                      base::UTF8ToUTF16(kLoadingPlaceholder));
  proxy_->AddSeparator();

  // Fetch Quick Answer.
  QuickAnswersRequest request;
  request.selected_text = selected_text;
  request.context.device_properties.language = GetDeviceLanguage();
  query_ = request.selected_text;
  quick_answers_client_->SendRequest(request);
}

void QuickAnswersMenuObserver::OnContextMenuShown(
    const content::ContextMenuParams& params,
    const gfx::Rect& bounds_in_screen) {
  if (!IsRichUiEnabled())
    return;

  if (!quick_answers_controller_)
    return;

  // Skip password input field.
  if (params.input_field_type ==
      blink::ContextMenuDataInputFieldType::kPassword) {
    return;
  }

  // Skip editable text selection if the feature is not enabled.
  if (params.is_editable &&
      !chromeos::features::IsQuickAnswersOnEditableTextEnabled()) {
    return;
  }

  // Skip if no text selected.
  auto selected_text = base::UTF16ToUTF8(params.selection_text);
  if (selected_text.empty())
    return;

  bounds_in_screen_ = bounds_in_screen;

  content::RenderFrameHost* focused_frame =
      proxy_->GetWebContents()->GetFocusedFrame();
  if (focused_frame) {
    quick_answers_controller_->SetPendingShowQuickAnswers();
    focused_frame->RequestTextSurroundingSelection(
        base::BindOnce(
            &QuickAnswersMenuObserver::OnTextSurroundingSelectionAvailable,
            weak_factory_.GetWeakPtr(), selected_text),
        kMaxSurroundingTextLength);
  }
}

void QuickAnswersMenuObserver::OnContextMenuViewBoundsChanged(
    const gfx::Rect& bounds_in_screen) {
  bounds_in_screen_ = bounds_in_screen;
  if (!quick_answers_controller_)
    return;
  quick_answers_controller_->UpdateQuickAnswersAnchorBounds(bounds_in_screen);
}

void QuickAnswersMenuObserver::OnMenuClosed() {
  if (!IsRichUiEnabled())
    return;

  if (!quick_answers_controller_)
    return;

  quick_answers_controller_->DismissQuickAnswers(!is_other_command_executed_);
}

bool QuickAnswersMenuObserver::IsCommandIdSupported(int command_id) {
  return (command_id == IDC_CONTENT_CONTEXT_QUICK_ANSWERS_INLINE_QUERY ||
          command_id == IDC_CONTENT_CONTEXT_QUICK_ANSWERS_INLINE_ANSWER);
}

bool QuickAnswersMenuObserver::IsCommandIdChecked(int command_id) {
  return false;
}

bool QuickAnswersMenuObserver::IsCommandIdEnabled(int command_id) {
  return command_id == IDC_CONTENT_CONTEXT_QUICK_ANSWERS_INLINE_QUERY;
}

bool QuickAnswersMenuObserver::IsRichUiEnabled() {
  return chromeos::features::IsQuickAnswersRichUiEnabled();
}

void QuickAnswersMenuObserver::ExecuteCommand(int command_id) {
  if (command_id == IDC_CONTENT_CONTEXT_QUICK_ANSWERS_INLINE_QUERY) {
    SendAssistantQuery(query_);

    quick_answers_client_->OnQuickAnswerClick(
        quick_answer_ ? quick_answer_->result_type : ResultType::kNoResult);
  }
}

void QuickAnswersMenuObserver::CommandWillBeExecuted(int command_id) {
  is_other_command_executed_ =
      command_id != IDC_CONTENT_CONTEXT_QUICK_ANSWERS_INLINE_QUERY;
}

void QuickAnswersMenuObserver::OnQuickAnswerReceived(
    std::unique_ptr<QuickAnswer> quick_answer) {
  if (quick_answer) {
    proxy_->UpdateMenuItem(IDC_CONTENT_CONTEXT_QUICK_ANSWERS_INLINE_ANSWER,
                           /*enabled=*/false,
                           /*hidden=*/false,
                           /*title=*/
                           TruncateString(quick_answer->primary_answer.empty()
                                              ? kNoResult
                                              : quick_answer->primary_answer));

    if (!quick_answer->secondary_answer.empty()) {
      proxy_->UpdateMenuItem(
          IDC_CONTENT_CONTEXT_QUICK_ANSWERS_INLINE_QUERY,
          /*enabled=*/true,
          /*hidden=*/false,
          /*title=*/TruncateString(quick_answer->secondary_answer));
    }
  } else {
    proxy_->UpdateMenuItem(IDC_CONTENT_CONTEXT_QUICK_ANSWERS_INLINE_ANSWER,
                           /*enabled=*/false,
                           /*hidden=*/false,
                           /*title=*/TruncateString(kNoResult));
  }
  quick_answer_ = std::move(quick_answer);
}

void QuickAnswersMenuObserver::OnNetworkError() {
  proxy_->UpdateMenuItem(IDC_CONTENT_CONTEXT_QUICK_ANSWERS_INLINE_ANSWER,
                         /*enabled=*/false,
                         /*hidden=*/false,
                         /*title=*/TruncateString(kNetworkError));
}

void QuickAnswersMenuObserver::OnEligibilityChanged(bool eligible) {
  is_eligible_ = eligible;
}

void QuickAnswersMenuObserver::SetQuickAnswerClientForTesting(
    std::unique_ptr<chromeos::quick_answers::QuickAnswersClient>
        quick_answers_client) {
  quick_answers_client_ = std::move(quick_answers_client);
}

void QuickAnswersMenuObserver::SendAssistantQuery(const std::string& query) {
  ash::AssistantInteractionController::Get()->StartTextInteraction(
      query, /*allow_tts=*/false,
      chromeos::assistant::AssistantQuerySource::kQuickAnswers);
}

std::string QuickAnswersMenuObserver::GetDeviceLanguage() {
  return l10n_util::GetLanguage(g_browser_process->GetApplicationLocale());
}

void QuickAnswersMenuObserver::OnTextSurroundingSelectionAvailable(
    const std::string& selected_text,
    const base::string16& surrounding_text,
    uint32_t start_offset,
    uint32_t end_offset) {
  Context context;
  context.surrounding_text = base::UTF16ToUTF8(surrounding_text);
  context.device_properties.language = GetDeviceLanguage();
  quick_answers_controller_->MaybeShowQuickAnswers(bounds_in_screen_,
                                                   selected_text, context);
}
