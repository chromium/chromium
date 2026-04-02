// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_selection_observer.h"

#include "base/containers/span.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/glic/glic_zero_state_suggestions_manager.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/grit/generated_resources.h"
#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/tabs/glic_nudge_controller.h"
#endif
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/text_elider.h"

namespace glic {

namespace {
// The minimum amount of time to wait between processing selection changes.
constexpr base::TimeDelta kSelectionProcessingDelay = base::Milliseconds(300);

// The maximum length of the selection text sent as a suggested prompt.
// Selections longer than this are ignored.
constexpr size_t kMaxSelectionLength = 1000;

#if !BUILDFLAG(IS_ANDROID)
// The MIME type for selected text.
constexpr char kSelectionMimeType[] = "application/x-glic-selection";
constexpr char kPromptMimeType[] = "application/x-glic-prompt";

#endif
}  // namespace

GlicSelectionObserver::GlicSelectionObserver(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  glic_keyed_service_ = GlicKeyedService::Get(profile);
}

GlicSelectionObserver::~GlicSelectionObserver() = default;

void GlicSelectionObserver::OnTextSelectionChanged(
    content::RenderFrameHost* render_frame_host,
    std::u16string_view selected_text) {
  if (!base::FeatureList::IsEnabled(features::kGlicSelectionPrompt)) {
    return;
  }

  if (selected_text.empty()) {
    pending_selection_text_ = std::u16string();
  } else if (selected_text.length() > kMaxSelectionLength) {
    pending_selection_text_ = std::u16string();
  } else {
    pending_selection_text_ = std::u16string(selected_text);
  }

  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeDelta time_since_last_process =
      now - last_selection_processing_time_;

  // Stop any existing timer. We will restart it if needed.
  selection_debounce_timer_.Stop();

  bool is_clearing = pending_selection_text_->empty();

  // If clearing, always debounce to avoid rapid clear-then-show sequences.
  if (is_clearing) {
    selection_debounce_timer_.Start(
        FROM_HERE, kSelectionProcessingDelay, this,
        &GlicSelectionObserver::ProcessPendingSelection);
    return;
  }

  // If showing, debounce if we processed another selection recently.
  if (time_since_last_process < kSelectionProcessingDelay) {
    selection_debounce_timer_.Start(
        FROM_HERE, kSelectionProcessingDelay - time_since_last_process, this,
        &GlicSelectionObserver::ProcessPendingSelection);
    return;
  }

  // Process immediately.
  ProcessPendingSelection();
}

void GlicSelectionObserver::ProcessPendingSelection() {
  if (!pending_selection_text_.has_value()) {
    return;
  }

  std::u16string selected_text = std::move(*pending_selection_text_);
  pending_selection_text_.reset();

  if (!selected_text.empty()) {
    last_selection_processing_time_ = base::TimeTicks::Now();
  }

  UpdateSelectionState(selected_text);
}

void GlicSelectionObserver::UpdateSelectionState(
    const std::u16string& selected_text) {
#if !BUILDFLAG(IS_ANDROID)
  auto* tab_interface = tabs::TabInterface::GetFromContents(web_contents());
  BrowserWindowInterface* bwi = tab_interface->GetBrowserWindowInterface();

  if (selected_text.empty()) {
    if (auto* controller = bwi->GetFeatures().glic_nudge_controller()) {
      controller->UpdateNudgeLabel(web_contents(), "", std::nullopt,
                                   /*anchored_message_text=*/std::string(),
                                   tabs::GlicNudgeActivity::kNudgeDismissed,
                                   base::DoNothing());
    }
    return;
  }

  bool panel_showing = false;
  if (glic_keyed_service_) {
    panel_showing = glic_keyed_service_->IsPanelShowingForBrowser(*bwi);
  }

  if (panel_showing) {
    auto context = mojom::AdditionalContext::New();
    context->source = mojom::AdditionalContextSource::kTextSelection;
    std::vector<mojom::AdditionalContextPartPtr> parts;

    {
      auto context_data = mojom::ContextData::New();
      context_data->mime_type = kSelectionMimeType;
      std::string utf8_text = base::UTF16ToUTF8(selected_text);
      context_data->data =
          mojo_base::BigBuffer(base::as_bytes(base::span(utf8_text)));
      parts.push_back(
          mojom::AdditionalContextPart::NewData(std::move(context_data)));
    }

    {
      auto context_data = mojom::ContextData::New();
      context_data->mime_type = kPromptMimeType;
      std::u16string prompt_text = l10n_util::GetStringFUTF16(
          IDS_GLIC_SELECTION_TELL_ME_ABOUT, std::u16string());
      std::string utf8_text = base::UTF16ToUTF8(prompt_text);
      context_data->data =
          mojo_base::BigBuffer(base::as_bytes(base::span(utf8_text)));
      parts.push_back(
          mojom::AdditionalContextPart::NewData(std::move(context_data)));
    }

    context->parts = std::move(parts);

    glic_keyed_service_->SendAdditionalContext(tab_interface->GetHandle(),
                                               std::move(context));
  } else {
    auto* controller = bwi->GetFeatures().glic_nudge_controller();
    if (controller) {
      std::u16string truncated_text;
      if (selected_text.length() <= 13) {
        truncated_text = selected_text;
      } else {
        truncated_text = gfx::StringSlicer(selected_text, gfx::kEllipsisUTF16,
                                           /*elide_in_middle=*/false,
                                           /*elide_at_beginning=*/false)
                             .CutString(13, /*insert_ellipsis=*/true);
      }
      std::u16string label = l10n_util::GetStringFUTF16(
          IDS_GLIC_SELECTION_ASK_ABOUT, truncated_text);
      std::u16string title = l10n_util::GetStringFUTF16(
          IDS_GLIC_SELECTION_TELL_ME_ABOUT, selected_text);
      controller->UpdateNudgeLabel(web_contents(), base::UTF16ToUTF8(label),
                                   std::make_optional(base::UTF16ToUTF8(title)),
                                   /*anchored_message_text=*/std::string(),
                                   std::nullopt, base::DoNothing());
    }
  }
#endif
}

}  // namespace glic
