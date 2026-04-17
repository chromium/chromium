// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_selection_observer.h"

#include "base/containers/span.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/glic/browser_ui/glic_selection_widget.h"
#include "chrome/browser/glic/glic_zero_state_suggestions_manager.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/glic_nudge_controller.h"
#include "chrome/grit/generated_resources.h"
#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/tabs/glic_nudge_controller.h"
#endif
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/text_elider.h"
#include "ui/views/widget/widget.h"

namespace glic {

namespace {
// The minimum amount of time to wait between processing selection changes.
constexpr base::TimeDelta kSelectionProcessingDelay = base::Milliseconds(300);

// The maximum length of the selection text sent as a suggested prompt.
// Selections longer than this are ignored.
constexpr size_t kMaxSelectionLength = 1000;

// The MIME type for selected text.
constexpr char kSelectionMimeType[] = "application/x-glic-selection";
constexpr char kPromptMimeType[] = "application/x-glic-prompt";

}  // namespace

GlicSelectionObserver::GlicSelectionObserver(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  glic_keyed_service_ = GlicKeyedService::Get(profile);

  web_contents->ForEachRenderFrameHost(
      [this](content::RenderFrameHost* render_frame_host) {
        if (auto* rwh = render_frame_host->GetRenderWidgetHost()) {
          rwh->AddInputEventObserver(this);
        }
      });
}

GlicSelectionObserver::~GlicSelectionObserver() {
  if (selection_widget_) {
    selection_widget_->CloseWithReason(views::Widget::ClosedReason::kLostFocus);
  }

  if (web_contents()) {
    web_contents()->ForEachRenderFrameHost(
        [this](content::RenderFrameHost* render_frame_host) {
          if (auto* rwh = render_frame_host->GetRenderWidgetHost()) {
            rwh->RemoveInputEventObserver(this);
          }
        });
  }
}

void GlicSelectionObserver::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  if (auto* rwh = render_frame_host->GetRenderWidgetHost()) {
    rwh->AddInputEventObserver(this);
  }
}

void GlicSelectionObserver::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  if (auto* rwh = render_frame_host->GetRenderWidgetHost()) {
    rwh->RemoveInputEventObserver(this);
  }
}

void GlicSelectionObserver::OnVisibilityChanged(
    content::Visibility visibility) {
  if (visibility == content::Visibility::HIDDEN && selection_widget_) {
    selection_widget_->CloseWithReason(views::Widget::ClosedReason::kLostFocus);
  }
}

void GlicSelectionObserver::PrimaryPageChanged(content::Page& page) {
  if (selection_widget_) {
    selection_widget_->CloseWithReason(views::Widget::ClosedReason::kLostFocus);
  }
}

void GlicSelectionObserver::OnInputEvent(
    const content::RenderWidgetHost& host,
    const blink::WebInputEvent& event,
    content::RenderWidgetHost::InputEventObserver::InputEventSource source) {
  if (event.GetType() == blink::WebInputEvent::Type::kMouseDown) {
    is_mouse_down_ = true;
    is_key_selection_ = false;
    bounds_retry_count_ = 0;
    if (selection_widget_) {
      selection_widget_->CloseWithReason(
          views::Widget::ClosedReason::kLostFocus);
    }
  } else if (event.GetType() == blink::WebInputEvent::Type::kMouseUp ||
             event.GetType() == blink::WebInputEvent::Type::kMouseLeave) {
    is_mouse_down_ = false;
    ProcessPendingSelection();
  } else if (event.GetType() ==
                 blink::WebInputEvent::Type::kGestureScrollBegin ||
             event.GetType() == blink::WebInputEvent::Type::kMouseWheel ||
             event.GetType() == blink::WebInputEvent::Type::kRawKeyDown ||
             event.GetType() == blink::WebInputEvent::Type::kKeyDown) {
    if (event.GetType() == blink::WebInputEvent::Type::kRawKeyDown ||
        event.GetType() == blink::WebInputEvent::Type::kKeyDown) {
      is_key_selection_ = true;
    }
    if (selection_widget_) {
      selection_widget_->CloseWithReason(
          views::Widget::ClosedReason::kLostFocus);
    }
  }
}

void GlicSelectionObserver::OnTextSelectionChanged(
    content::RenderFrameHost* render_frame_host,
    std::u16string_view selected_text) {
  if (!base::FeatureList::IsEnabled(features::kGlicSelectionPrompt)) {
    return;
  }

  if (is_key_selection_) {
    pending_selection_text_ = std::u16string();
    UpdateSelectionState(std::u16string());
    return;
  }

  bounds_retry_count_ = 0;

  if (selected_text.empty()) {
    pending_selection_text_ = std::u16string();
  } else if (selected_text.length() > kMaxSelectionLength) {
    pending_selection_text_ = std::u16string();
  } else {
    pending_selection_text_ = std::u16string(selected_text);
  }
  if (render_frame_host) {
    last_selection_frame_id_ = render_frame_host->GetGlobalId();
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

  // Copy the text rather than std::move. If the user drags for >300ms, this
  // method runs while the mouse is down, and we need the text to remain intact
  // in the optional so it can be processed again on mouse-up.
  std::u16string selected_text = *pending_selection_text_;
  if (!is_mouse_down_) {
    pending_selection_text_.reset();
  }

  if (!selected_text.empty()) {
    last_selection_processing_time_ = base::TimeTicks::Now();
  }

  UpdateSelectionState(selected_text);
}

// static
void GlicSelectionObserver::InvokeGlicFromSelectionWidget(
    std::string prompt_text,
    base::WeakPtr<content::WebContents> web_contents) {
  if (web_contents) {
    if (auto* tab_interface =
            tabs::TabInterface::MaybeGetFromContents(web_contents.get())) {
      if (auto* bwi = tab_interface->GetBrowserWindowInterface()) {
        Profile* profile =
            Profile::FromBrowserContext(web_contents->GetBrowserContext());
        if (auto* glic_keyed_service = GlicKeyedService::Get(profile)) {
          glic_keyed_service->ToggleUI(
              bwi, false, mojom::InvocationSource::kNudge, prompt_text);
        }
      }
    }
  }
}

void GlicSelectionObserver::UpdateSelectionState(
    const std::u16string& selected_text) {
  auto* tab_interface =
      tabs::TabInterface::MaybeGetFromContents(web_contents());
  if (!tab_interface) {
    return;
  }
  BrowserWindowInterface* bwi = tab_interface->GetBrowserWindowInterface();

  if (selected_text.empty() || web_contents()->IsFocusedElementEditable()) {
    if (selection_widget_) {
      selection_widget_->CloseWithReason(
          views::Widget::ClosedReason::kLostFocus);
    }
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
    if (selection_widget_) {
      selection_widget_->CloseWithReason(
          views::Widget::ClosedReason::kLostFocus);
    }

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
    ShowSelectionAffordance(selected_text, bwi);
  }
}

void GlicSelectionObserver::ShowSelectionAffordance(
    const std::u16string& selected_text,
    BrowserWindowInterface* bwi) {
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

    if (!features::kGlicSelectionPromptUseWidget.Get()) {
      // Show selection nudge
      controller->UpdateNudgeLabel(web_contents(), base::UTF16ToUTF8(label),
                                   std::make_optional(base::UTF16ToUTF8(title)),
                                   /*anchored_message_text=*/std::string(),
                                   std::nullopt, base::DoNothing());
    } else {
      // Show selection widget
      if (!is_mouse_down_) {
        // Find the RenderFrameHost that has the selection.
        content::RenderFrameHost* selected_frame =
            content::RenderFrameHost::FromID(last_selection_frame_id_);
        std::optional<gfx::Rect> bounds =
            web_contents()->GetTextSelectionBounds(selected_frame);
        if (bounds.has_value() && !bounds->IsEmpty()) {
          if (selection_widget_) {
            selection_widget_->CloseWithReason(
                views::Widget::ClosedReason::kLostFocus);
          }

          auto invoke_glic = base::BindRepeating(
              &GlicSelectionObserver::InvokeGlicFromSelectionWidget,
              base::UTF16ToUTF8(selected_text), web_contents()->GetWeakPtr());

          selection_widget_ =
              GlicSelectionWidgetDelegate::Show(web_contents(), *bounds,
                                                std::move(invoke_glic))
                  ->GetWeakPtr();
        } else if (bounds_retry_count_ < 5) {
          // Retry showing the widget, bounds might not be available yet due
          // to IPC timing (especially on double click).
          bounds_retry_count_++;
          pending_selection_text_ = selected_text;
          selection_debounce_timer_.Start(
              FROM_HERE, base::Milliseconds(100), this,
              &GlicSelectionObserver::ProcessPendingSelection);
        }
      }
    }
  }
}

}  // namespace glic
