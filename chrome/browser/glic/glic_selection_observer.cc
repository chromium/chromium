// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_selection_observer.h"

#include "base/containers/span.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/glic/browser_ui/glic_nudge_controller.h"
#include "chrome/browser/glic/browser_ui/glic_selection_widget.h"
#include "chrome/browser/glic/glic_zero_state_suggestions_manager.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/grit/generated_resources.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/text_elider.h"
#include "ui/views/widget/widget.h"

namespace glic {

namespace {
// The minimum amount of time to wait between processing selection changes.
constexpr base::TimeDelta kSelectionProcessingDelay = base::Milliseconds(200);

// The maximum length of the selection text sent as a suggested prompt.
// Selections longer than this are ignored.
constexpr size_t kMaxSelectionLength = 1000;

// The minimum length of the selection text sent as a suggested prompt.
// Selections shorter than this are ignored.
constexpr size_t kMinSelectionLength = 3;

// The MIME type for selected text.
constexpr char kSelectionMimeType[] = "application/x-glic-selection";

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(GlicSelectionAction)
enum class GlicSelectionAction {
  kNudgeShown = 0,
  kWidgetShown = 1,
  kNudgeClicked = 2,
  kWidgetClicked = 3,
  kMaxValue = kWidgetClicked
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicSelectionAction)

}  // namespace

GlicSelectionObserver::GlicSelectionObserver(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  glic_keyed_service_ = GlicKeyedService::Get(profile);

  if (base::FeatureList::IsEnabled(features::kGlicSelectionPrompt)) {
    web_contents->ForEachRenderFrameHost(
        [this](content::RenderFrameHost* render_frame_host) {
          RenderFrameCreated(render_frame_host);
        });
  }
}

GlicSelectionObserver::~GlicSelectionObserver() {
  if (selection_widget_) {
    selection_widget_->CloseWithReason(views::Widget::ClosedReason::kLostFocus);
  }

  for (const auto& [frame_id, rwh] : rwh_by_frame_) {
    if (rwh) {
      rwh->RemoveInputEventObserver(this);
    }
  }
  rwh_by_frame_.clear();
}

void GlicSelectionObserver::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  if (!base::FeatureList::IsEnabled(features::kGlicSelectionPrompt)) {
    return;
  }
  if (auto* rwh = render_frame_host->GetRenderWidgetHost()) {
    if (rwh_by_frame_.insert({render_frame_host->GetGlobalId(), rwh}).second) {
      rwh->AddInputEventObserver(this);
    }
  }
}

void GlicSelectionObserver::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  auto it = rwh_by_frame_.find(render_frame_host->GetGlobalId());
  if (it != rwh_by_frame_.end()) {
    if (it->second) {
      it->second->RemoveInputEventObserver(this);
    }
    rwh_by_frame_.erase(it);
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

void GlicSelectionObserver::OnWebContentsLostFocus(
    content::RenderWidgetHost* render_widget_host) {
  // If the web contents loses focus, process any pending selection immediately.
  if (selection_debounce_timer_.IsRunning()) {
    selection_debounce_timer_.Stop();
    ProcessPendingSelection();
  }
}

void GlicSelectionObserver::OnInputEvent(
    const content::RenderWidgetHost& host,
    const blink::WebInputEvent& event,
    content::RenderWidgetHost::InputEventObserver::InputEventSource source) {
  if (!base::FeatureList::IsEnabled(features::kGlicSelectionPrompt)) {
    return;
  }

  auto dismiss_ui = [this]() {
    if (selection_widget_) {
      selection_widget_->CloseWithReason(
          views::Widget::ClosedReason::kLostFocus);
    }
    if (auto* tab_interface =
            tabs::TabInterface::MaybeGetFromContents(web_contents())) {
      if (auto* bwi = tab_interface->GetBrowserWindowInterface()) {
        if (auto* controller = bwi->GetFeatures().glic_nudge_controller()) {
          controller->UpdateNudgeLabel(web_contents(), "", std::nullopt,
                                       /*anchored_message_text=*/std::string(),
                                       GlicNudgeActivity::kNudgeDismissed,
                                       base::DoNothing());
        }
      }
    }
  };

  switch (event.GetType()) {
    case blink::WebInputEvent::Type::kMouseDown:
    case blink::WebInputEvent::Type::kPointerDown:
    case blink::WebInputEvent::Type::kGestureTapDown:
    case blink::WebInputEvent::Type::kTouchStart: {
      bool is_left_click_or_touch = true;
      if (event.GetType() == blink::WebInputEvent::Type::kMouseDown ||
          event.GetType() == blink::WebInputEvent::Type::kPointerDown) {
        const auto& mouse_event =
            static_cast<const blink::WebMouseEvent&>(event);
        if (mouse_event.button != blink::WebPointerProperties::Button::kLeft) {
          is_left_click_or_touch = false;
        }
      }

      is_key_selection_ = false;
      bounds_retry_count_ = 0;
      dismiss_ui();

      // Workaround for a bug in Blink: when a user single-clicks directly on
      // top of an existing selection, Blink collapses the selection on MouseUp
      // but fails to send the corresponding OnTextSelectionChanged(empty) IPC.
      // Since any left-click or touch tap invalidates the current static text
      // selection (by either placing the caret, clearing the selection, or
      // initiating a new drag), we preemptively clear the context here to
      // ensure it is not left hanging.
      if (is_left_click_or_touch) {
        pending_selection_text_.reset();
        if (selection_debounce_timer_.IsRunning()) {
          selection_debounce_timer_.Stop();
        }
        if (has_sent_selection_context_) {
          UpdateSelectionState(std::u16string());
        }
      }
      break;
    }

    case blink::WebInputEvent::Type::kMouseUp:
    case blink::WebInputEvent::Type::kPointerUp:
    case blink::WebInputEvent::Type::kPointerCancel:
    case blink::WebInputEvent::Type::kTouchEnd:
    case blink::WebInputEvent::Type::kTouchCancel:
    case blink::WebInputEvent::Type::kGestureTapCancel:
      // If the user lifts their finger/mouse and we have a pending selection
      // timer, trigger it instantly so the UI feels perfectly responsive.
      if (selection_debounce_timer_.IsRunning()) {
        selection_debounce_timer_.Stop();
        ProcessPendingSelection();
      }
      break;

    case blink::WebInputEvent::Type::kRawKeyDown:
    case blink::WebInputEvent::Type::kKeyDown:
      is_key_selection_ = true;
      dismiss_ui();
      break;

    case blink::WebInputEvent::Type::kGestureScrollBegin:
    case blink::WebInputEvent::Type::kMouseWheel:
      dismiss_ui();
      break;

    default:
      break;
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

  if (selected_text.length() < kMinSelectionLength ||
      selected_text.length() > kMaxSelectionLength) {
    pending_selection_text_ = std::u16string();
  } else {
    pending_selection_text_ = std::u16string(selected_text);
  }
  if (render_frame_host) {
    last_selection_frame_id_ = render_frame_host->GetGlobalId();
  }

  // Always debounce selection changes. If the user is actively dragging,
  // this timer will be continually pushed back until they pause or release
  // the mouse. If the mouse is released cleanly, OnInputEvent intercepts the
  // MouseUp and triggers ProcessPendingSelection instantly. If the OS drops
  // the MouseUp event, the timer will naturally fire 200ms after they stop
  // dragging.
  selection_debounce_timer_.Start(
      FROM_HERE, kSelectionProcessingDelay, this,
      &GlicSelectionObserver::ProcessPendingSelection);
}

void GlicSelectionObserver::ProcessPendingSelection() {
  if (!pending_selection_text_.has_value()) {
    return;
  }

  std::u16string selected_text = std::move(*pending_selection_text_);
  pending_selection_text_.reset();
  selection_debounce_timer_.Stop();

  UpdateSelectionState(selected_text);
}

// static
void GlicSelectionObserver::InvokeGlicFromSelectionAffordance(
    std::u16string selected_text,
    bool is_widget,
    base::WeakPtr<content::WebContents> web_contents,
    GlicNudgeActivity activity) {
  if (activity != GlicNudgeActivity::kNudgeClicked) {
    return;
  }

  bool is_post_fre = false;
  if (web_contents) {
    Profile* profile =
        Profile::FromBrowserContext(web_contents->GetBrowserContext());
    is_post_fre = GlicEnabling::HasConsentedForProfile(profile);
  }

  const char* histogram_suffix = is_post_fre ? ".PostFre" : ".PreFre";

  base::UmaHistogramEnumeration(
      base::StrCat({"Glic.Selection.Action", histogram_suffix}),
      is_widget ? GlicSelectionAction::kWidgetClicked
                : GlicSelectionAction::kNudgeClicked);
  if (is_widget) {
    base::UmaHistogramCounts1000(
        base::StrCat(
            {"Glic.Selection.WidgetClicked.SelectionLength", histogram_suffix}),
        selected_text.length());
  } else {
    base::UmaHistogramCounts1000(
        base::StrCat(
            {"Glic.Selection.NudgeClicked.SelectionLength", histogram_suffix}),
        selected_text.length());
  }

  if (web_contents) {
    if (auto* tab_interface =
            tabs::TabInterface::MaybeGetFromContents(web_contents.get())) {
      if (tab_interface->GetBrowserWindowInterface()) {
        Profile* profile =
            Profile::FromBrowserContext(web_contents->GetBrowserContext());
        if (auto* glic_keyed_service = GlicKeyedService::Get(profile)) {
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

          context->parts = std::move(parts);

          GlicInvokeOptions options(mojom::InvocationSource::kNudge);
          options.additional_context = std::move(context);

          glic_keyed_service->Invoke(tab_interface, std::move(options));
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
                                   GlicNudgeActivity::kNudgeDismissed,
                                   base::DoNothing());
    }

    if (has_sent_selection_context_ && glic_keyed_service_) {
      if (glic_keyed_service_->GetInstanceForTab(tab_interface)) {
        auto context = mojom::AdditionalContext::New();
        context->source = mojom::AdditionalContextSource::kTextSelection;
        context->parts = std::vector<mojom::AdditionalContextPartPtr>();
        glic_keyed_service_->SendAdditionalContext(tab_interface->GetHandle(),
                                                   std::move(context));
      }
      has_sent_selection_context_ = false;
    }

    return;
  }

  bool panel_showing = false;
  if (glic_keyed_service_ &&
      glic_keyed_service_->GetInstanceForTab(tab_interface)) {
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

    context->parts = std::move(parts);

    glic_keyed_service_->SendAdditionalContext(tab_interface->GetHandle(),
                                               std::move(context));
    has_sent_selection_context_ = true;
  } else {
    ShowSelectionAffordance(selected_text, bwi);
    has_sent_selection_context_ = false;
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

    bool is_post_fre = GlicEnabling::HasConsentedForProfile(
        Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
    const char* histogram_suffix = is_post_fre ? ".PostFre" : ".PreFre";

    if (!features::kGlicSelectionPromptUseWidget.Get()) {
      // Show selection nudge
      base::UmaHistogramEnumeration(
          base::StrCat({"Glic.Selection.Action", histogram_suffix}),
          GlicSelectionAction::kNudgeShown);
      auto invoke_glic = base::BindRepeating(
          &GlicSelectionObserver::InvokeGlicFromSelectionAffordance,
          selected_text,
          /*is_widget=*/false, web_contents()->GetWeakPtr());
      controller->UpdateNudgeLabel(web_contents(), base::UTF16ToUTF8(label),
                                   std::nullopt,
                                   /*anchored_message_text=*/std::string(),
                                   std::nullopt, std::move(invoke_glic));
    } else {
      // Show selection widget
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

        base::UmaHistogramEnumeration(
            base::StrCat({"Glic.Selection.Action", histogram_suffix}),
            GlicSelectionAction::kWidgetShown);
        auto invoke_glic = base::BindRepeating(
            &GlicSelectionObserver::InvokeGlicFromSelectionAffordance,
            selected_text,
            /*is_widget=*/true, web_contents()->GetWeakPtr(),
            GlicNudgeActivity::kNudgeClicked);

        selection_widget_ =
            GlicSelectionWidgetDelegate::Show(
                web_contents(), *bounds,
                base::BindRepeating(
                    [](base::RepeatingCallback<void()> invoke_glic_cb) {
                      invoke_glic_cb.Run();
                    },
                    std::move(invoke_glic)))
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

}  // namespace glic
