// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/quick_answers_menu_observer.h"

#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/assistant/controller/assistant_interaction_controller.h"
#include "ash/public/cpp/quick_answers/controller/quick_answers_controller.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/branding_buildflags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/services/assistant/public/cpp/assistant_service.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/renderer_context_menu/render_view_context_menu_proxy.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/context_menu/context_menu.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/text_constants.h"
#include "ui/gfx/text_elider.h"

namespace {

using chromeos::quick_answers::Context;
using chromeos::quick_answers::QuickAnswersClient;

constexpr int kMaxSurroundingTextLength = 300;

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

void QuickAnswersMenuObserver::OnContextMenuShown(
    const content::ContextMenuParams& params,
    const gfx::Rect& bounds_in_screen) {
  menu_shown_time_ = base::TimeTicks::Now();

  if (!quick_answers_controller_ || !is_eligible_)
    return;

  // Skip password input field.
  if (params.input_field_type ==
      blink::mojom::ContextMenuDataInputFieldType::kPassword) {
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
  const base::TimeDelta time_since_request_sent =
      base::TimeTicks::Now() - menu_shown_time_;
  if (is_other_command_executed_) {
    base::UmaHistogramTimes("QuickAnswers.ContextMenu.Close.DurationWithClick",
                            time_since_request_sent);
  } else {
    base::UmaHistogramTimes(
        "QuickAnswers.ContextMenu.Close.DurationWithoutClick",
        time_since_request_sent);
  }

  base::UmaHistogramBoolean("QuickAnswers.ContextMenu.Close",
                            is_other_command_executed_);

  if (!quick_answers_controller_)
    return;

  quick_answers_controller_->DismissQuickAnswers(!is_other_command_executed_);
}

void QuickAnswersMenuObserver::CommandWillBeExecuted(int command_id) {
  is_other_command_executed_ = true;
}

void QuickAnswersMenuObserver::OnEligibilityChanged(bool eligible) {
  is_eligible_ = eligible;
}

void QuickAnswersMenuObserver::OnTextSurroundingSelectionAvailable(
    const std::string& selected_text,
    const std::u16string& surrounding_text,
    uint32_t start_offset,
    uint32_t end_offset) {
  PrefService* prefs =
      Profile::FromBrowserContext(proxy_->GetBrowserContext())->GetPrefs();

  Context context;
  context.surrounding_text = base::UTF16ToUTF8(surrounding_text);
  context.device_properties.language =
      l10n_util::GetLanguage(g_browser_process->GetApplicationLocale());
  context.device_properties.preferred_languages =
      prefs->GetString(language::prefs::kPreferredLanguages);
  quick_answers_controller_->MaybeShowQuickAnswers(bounds_in_screen_,
                                                   selected_text, context);
}
