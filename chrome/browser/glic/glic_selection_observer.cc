// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_selection_observer.h"

#include "base/containers/flat_set.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "chrome/browser/glic/browser_ui/glic_selection_widget_controller.h"
#include "chrome/browser/glic/common/local_hotkey_manager.h"
#include "chrome/browser/glic/glic_zero_state_suggestions_manager.h"
#include "chrome/browser/glic/host/context/glic_sharing_utils.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_instance.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_side_panel_coordinator.h"
#include "chrome/browser/glic/public/service/glic_instance_coordinator.h"
#include "chrome/browser/glic/selection/shake_trigger.h"
#include "chrome/browser/glic/selection/text_selection_context.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/page_context_eligibility_helper.h"
#include "chrome/common/webui_url_constants.h"
#include "components/optimization_guide/content/browser/page_context_eligibility.h"
#include "components/optimization_guide/content/browser/page_context_eligibility_observer.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_utils.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace glic {

namespace {

// The maximum length of the selection text sent as a suggested prompt.
// Selections longer than this are ignored.
constexpr size_t kMaxSelectionLength = 1000;

// The minimum length of the selection text sent as a suggested prompt.
// Selections shorter than this are ignored.
constexpr size_t kMinSelectionLength = 3;

bool IsListenedToInputEvent(blink::WebInputEvent::Type type) {
  switch (type) {
    case blink::WebInputEvent::Type::kMouseDown:
    case blink::WebInputEvent::Type::kPointerDown:
    case blink::WebInputEvent::Type::kGestureTapDown:
    case blink::WebInputEvent::Type::kTouchStart:
    case blink::WebInputEvent::Type::kMouseUp:
    case blink::WebInputEvent::Type::kPointerUp:
    case blink::WebInputEvent::Type::kPointerCancel:
    case blink::WebInputEvent::Type::kTouchEnd:
    case blink::WebInputEvent::Type::kTouchCancel:
    case blink::WebInputEvent::Type::kGestureTapCancel:
    case blink::WebInputEvent::Type::kKeyUp:
    case blink::WebInputEvent::Type::kRawKeyDown:
    case blink::WebInputEvent::Type::kKeyDown:
    case blink::WebInputEvent::Type::kGestureScrollBegin:
    case blink::WebInputEvent::Type::kMouseWheel:
    case blink::WebInputEvent::Type::kMouseMove:
      return true;
    default:
      return false;
  }
}

}  // namespace

DEFINE_USER_DATA(GlicSelectionObserver);

// static
GlicSelectionObserver* GlicSelectionObserver::From(tabs::TabInterface* tab) {
  return tab ? Get(tab->GetUnownedUserDataHost()) : nullptr;
}

GlicSelectionObserver::GlicSelectionObserver(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  CHECK(web_contents);
  shake_trigger_ = std::make_unique<ShakeTrigger>(web_contents, *this);
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  glic_keyed_service_ = GlicKeyedService::Get(profile);

  if (glic_keyed_service_) {
    panel_state_subscription_ =
        glic_keyed_service_->instance_coordinator().AddGlobalShowHideCallback(
            base::BindRepeating(&GlicSelectionObserver::OnGlobalPanelShowHide,
                                weak_ptr_factory_.GetWeakPtr()));
  }

  std::string account;
  if (profile) {
    auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
    if (identity_manager) {
      account =
          identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
              .email;
    }
  }

  auto* tab_interface = tabs::TabInterface::MaybeGetFromContents(web_contents);
  if (tab_interface) {
    scoped_unowned_user_data_ =
        std::make_unique<ui::ScopedUnownedUserData<GlicSelectionObserver>>(
            tab_interface->GetUnownedUserDataHost(), *this);
  }
  auto* helper = tab_interface
                     ? tabs::PageContextEligibilityHelper::From(tab_interface)
                     : nullptr;
  if (helper) {
    page_context_eligibility_subscription_ =
        helper->RegisterEligibilityChangeCallback(base::BindRepeating(
            &GlicSelectionObserver::OnPageContextEligibilityChanged,
            weak_ptr_factory_.GetWeakPtr()));
  } else {
    CreatePageContextEligibilityAPI(std::move(account));
  }

  widget_controller_ =
      std::make_unique<GlicSelectionWidgetController>(web_contents, *this);

  web_contents->ForEachRenderFrameHost(
      [this](content::RenderFrameHost* render_frame_host) {
        RenderFrameCreated(render_frame_host);
      });
}

GlicSelectionObserver::~GlicSelectionObserver() {
  widget_controller_.reset();

  base::flat_set<content::RenderWidgetHost*> unique_rwhs;
  for (const auto& frame_token : observed_frames_) {
    content::RenderFrameHost* rfh =
        content::RenderFrameHost::FromFrameToken(frame_token);
    if (rfh && rfh->GetRenderWidgetHost()) {
      unique_rwhs.insert(rfh->GetRenderWidgetHost());
    }
  }
  for (auto* rwh : unique_rwhs) {
    rwh->RemoveInputEventObserver(this);
  }
  observed_frames_.clear();
}

void GlicSelectionObserver::OnTextSelectionChanged(
    content::RenderFrameHost* render_frame_host,
    std::u16string_view selected_text) {
  if (!IsTextSelectionSharingEnabled() && !has_sent_selection_context_) {
    return;
  }

  // On unshareable pages (such as chrome:// URLs), automatic selection sharing
  // is disabled. However, if context was explicitly sent (e.g. via the context
  // menu), any deselection must clear that context from the panel.
  if (!IsTabValidForSharing(web_contents())) {
    if (has_sent_selection_context_) {
      UpdateSelectionState(std::u16string(), /*is_pending_selection=*/false,
                           SelectionSource::kAutomatic);
    }
    return;
  }

  if (web_contents()->IsFocusedElementEditable()) {
    selected_text = std::u16string_view();
  }

  bounds_retry_count_ = 0;

  std::u16string_view trimmed_text =
      base::TrimWhitespace(selected_text, base::TRIM_ALL);

  if (trimmed_text.length() > kMaxSelectionLength) {
    pending_selection_text_ = std::u16string();
  } else {
    size_t non_whitespace_count = 0;
    bool exceeds_minimum_selection_length = false;
    for (char16_t c : trimmed_text) {
      if (!base::IsUnicodeWhitespace(c)) {
        non_whitespace_count++;
      }
      if (non_whitespace_count == kMinSelectionLength) {
        exceeds_minimum_selection_length = true;
        break;
      }
    }

    if (!exceeds_minimum_selection_length) {
      pending_selection_text_ = std::u16string();
    } else {
      pending_selection_text_ = std::u16string(trimmed_text);
    }
  }
  if (render_frame_host) {
    last_selection_frame_token_ = render_frame_host->GetGlobalFrameToken();
  }

  // If not in the process of selecting, process the selection immediately.
  if (!is_selecting_) {
    ProcessPendingSelection();
  }
}

content::RenderFrameHost* GlicSelectionObserver::GetSelectedFrame() const {
  if (!web_contents() || !last_selection_frame_token_.has_value()) {
    return nullptr;
  }
  return content::RenderFrameHost::FromFrameToken(*last_selection_frame_token_);
}

std::optional<gfx::Rect> GlicSelectionObserver::GetCurrentSelectionBounds()
    const {
  if (auto* selected_frame = GetSelectedFrame()) {
    std::optional<gfx::Rect> bounds =
        web_contents()->GetTextSelectionBounds(selected_frame);
    if (bounds.has_value() && !bounds->IsEmpty()) {
      return bounds;
    }
  }
  return std::nullopt;
}

const std::u16string& GlicSelectionObserver::GetSelectedText() const {
  return last_selected_text_;
}

void GlicSelectionObserver::UpdateSelectionStateFromContextMenu(
    const std::u16string& selected_text) {
  UpdateSelectionState(selected_text, /*is_pending_selection=*/false,
                       SelectionSource::kContextMenu);
}

void GlicSelectionObserver::DismissUI(DismissReason reason) {
  widget_controller_->Dismiss(reason);
}

bool GlicSelectionObserver::IsTextSelectionSharingEnabled() const {
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  if (!identity_manager ||
      !identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    return false;
  }
  return GlicEnabling::IsEnabledForProfile(profile);
}

bool GlicSelectionObserver::IsSidePanelOpen() const {
  auto* tab_interface =
      tabs::TabInterface::MaybeGetFromContents(web_contents());
  return tab_interface && GlicSidePanelCoordinator::IsShowing(tab_interface);
}

void GlicSelectionObserver::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  if (auto* rwh = render_frame_host->GetRenderWidgetHost()) {
    bool already_observing = false;
    for (const auto& frame_token : observed_frames_) {
      content::RenderFrameHost* rfh =
          content::RenderFrameHost::FromFrameToken(frame_token);
      if (rfh && rfh->GetRenderWidgetHost() == rwh) {
        already_observing = true;
        break;
      }
    }
    if (observed_frames_.insert(render_frame_host->GetGlobalFrameToken())
            .second) {
      if (!already_observing) {
        rwh->AddInputEventObserver(this);
      }
    }
  }
}

void GlicSelectionObserver::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  if (!observed_frames_.contains(render_frame_host->GetGlobalFrameToken())) {
    return;
  }

  content::RenderWidgetHost* rwh = render_frame_host->GetRenderWidgetHost();
  observed_frames_.erase(render_frame_host->GetGlobalFrameToken());

  bool still_observing = false;
  for (const auto& frame_token : observed_frames_) {
    content::RenderFrameHost* rfh =
        content::RenderFrameHost::FromFrameToken(frame_token);
    if (rfh && rfh->GetRenderWidgetHost() == rwh) {
      still_observing = true;
      break;
    }
  }
  if (!still_observing && rwh) {
    rwh->RemoveInputEventObserver(this);
  }
}

void GlicSelectionObserver::OnVisibilityChanged(
    content::Visibility visibility) {
  if (visibility == content::Visibility::HIDDEN) {
    widget_controller_->Close();
  }
}

void GlicSelectionObserver::PrimaryPageChanged(content::Page& page) {
  ResetSelectionState();
}

void GlicSelectionObserver::PrimaryMainFrameWasResized(bool width_changed) {
  DismissUI(DismissReason::kExternal);
}

void GlicSelectionObserver::OnWebContentsLostFocus(
    content::RenderWidgetHost* render_widget_host) {
  if (web_contents()->IsBeingDestroyed()) {
    ResetPendingSelection();
    return;
  }

  // If the web contents loses focus, process any pending selection immediately.
  ProcessPendingSelection();
}

void GlicSelectionObserver::OnInputEvent(
    const content::RenderWidgetHost& host,
    const blink::WebInputEvent& event,
    content::RenderWidgetHost::InputEventObserver::InputEventSource source) {
  if (!IsListenedToInputEvent(event.GetType())) {
    return;
  }
  // Only the shake detector uses mouse moves. Skip them when it's off, so
  // that each mouse move doesn't post a task.
  if (event.GetType() == blink::WebInputEvent::Type::kMouseMove &&
      !shake_trigger_->IsEnabled()) {
    return;
  }
  // If text selection context was previously sent to the panel (e.g. via the
  // "Ask Gemini" context menu), we must still process input events even on
  // unshareable pages (such as chrome:// URLs) so that clicking away can clear
  // the selection chip from the panel.
  if (!IsTabValidForSharing(web_contents()) && !has_sent_selection_context_) {
    return;
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&GlicSelectionObserver::ProcessInputEvent,
                                weak_ptr_factory_.GetWeakPtr(), event.Clone()));
}

void GlicSelectionObserver::UpdateSelectionState(
    const std::u16string& selected_text,
    bool is_pending_selection,
    SelectionSource source) {
  last_selected_text_ = selected_text;
  auto* tab_interface =
      tabs::TabInterface::MaybeGetFromContents(web_contents());
  if (!tab_interface) {
    return;
  }

  if (source == SelectionSource::kContextMenu) {
    has_sent_selection_context_ = !selected_text.empty();
    return;
  }

  BrowserWindowInterface* bwi = tab_interface->GetBrowserWindowInterface();

  if (selected_text.empty()) {
    widget_controller_->Close();

    if (has_sent_selection_context_) {
      SendAdditionalContextToPanel(tab_interface, u"");
      has_sent_selection_context_ = false;
    }

    last_selection_frame_token_.reset();
    return;
  }

  if (!bwi) {
    return;
  }

  bool panel_showing = IsPanelShowing(tab_interface, bwi);
  bool show_widget =
      is_pending_selection && IsInlineCueEnabled() &&
      // Only normal browser windows show the widget, not popups or app windows.
      bwi->GetType() == BrowserWindowInterface::Type::TYPE_NORMAL;

  if (panel_showing) {
    if (show_widget) {
      ShowSelectionAffordance(selected_text);
    } else {
      widget_controller_->Close();
    }

    SendAdditionalContextToPanel(tab_interface, selected_text);
    has_sent_selection_context_ = true;
  } else {
    if (show_widget) {
      ShowSelectionAffordance(selected_text);
    }
    has_sent_selection_context_ = false;
  }
}

bool GlicSelectionObserver::IsInlineCueEnabled() const {
  if (!IsTextSelectionSharingEnabled()) {
    return false;
  }
  return GlicEnabling::IsInlineCueEnabledForProfile(
      Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
}

bool GlicSelectionObserver::IsPanelShowing(tabs::TabInterface* tab_interface,
                                           BrowserWindowInterface* bwi) {
  if (glic_keyed_service_ &&
      glic_keyed_service_->GetInstanceForTab(tab_interface)) {
    return glic_keyed_service_->IsPanelShowingForBrowser(*bwi);
  }
  return false;
}

void GlicSelectionObserver::SendAdditionalContextToPanel(
    tabs::TabInterface* tab_interface,
    const std::u16string& selected_text) {
  if (!glic_keyed_service_) {
    return;
  }

  // If the page is not eligible, do not send the additional context.
  if (!IsPageContextEligible() && !selected_text.empty()) {
    return;
  }

  GlicInvokeOptions options(glic::Target(*tab_interface),
                            mojom::InvocationSource::kTextSelectionWidget);
  // Delivering the selection shouldn't relocate the conversation: leave it on
  // the surface it's already showing on, and if that surface is a live mode
  // floaty, don't invoke at all (see `InvokeGlicFromSelectionAffordance`).
  options.preserve_active_surface = true;
  options.target.live_mode_behavior = LiveModeBehavior::kFail;
  options.additional_context = AdditionalTabContext(
      CreateTextSelectionContext(web_contents(), selected_text),
      content::GlobalRenderFrameHostId(), PolicyCheck::kNone);
  glic_keyed_service_->Invoke(std::move(options));
}

void GlicSelectionObserver::ShowSelectionAffordance(
    const std::u16string& selected_text) {
  GlicSelectionWidgetController::ShowResult result =
      widget_controller_->Show(selected_text);
  if (result == GlicSelectionWidgetController::ShowResult::kNoBounds &&
      bounds_retry_count_ < 5) {
    // Retry showing the widget, bounds might not be available yet due
    // to IPC timing (especially on double click).
    bounds_retry_count_++;
    pending_selection_text_ = selected_text;
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&GlicSelectionObserver::ProcessPendingSelection,
                       weak_ptr_factory_.GetWeakPtr()),
        base::Milliseconds(100));
  }
}

void GlicSelectionObserver::OnPageContextEligibilityChanged(
    optimization_guide::PageContextEligibilityStatus status) {
  // If the page context transitions into a liminal state (kUnknown) or becomes
  // ineligible (kNotEligible), and we've already sent selection context, we
  // should clear it.
  if (status != optimization_guide::PageContextEligibilityStatus::kEligible &&
      has_sent_selection_context_) {
    auto* tab_interface =
        tabs::TabInterface::MaybeGetFromContents(web_contents());
    if (tab_interface) {
      SendAdditionalContextToPanel(tab_interface, std::u16string());
      has_sent_selection_context_ = false;
    }
  } else if (status ==
                 optimization_guide::PageContextEligibilityStatus::kEligible &&
             !last_selected_text_.empty()) {
    auto* tab_interface =
        tabs::TabInterface::MaybeGetFromContents(web_contents());
    if (!tab_interface) {
      return;
    }
    BrowserWindowInterface* bwi = tab_interface->GetBrowserWindowInterface();
    if (bwi && IsPanelShowing(tab_interface, bwi)) {
      SendAdditionalContextToPanel(tab_interface, last_selected_text_);
      has_sent_selection_context_ = true;
    }
  }
}

bool GlicSelectionObserver::IsPageContextEligible() const {
  auto* tab_interface =
      tabs::TabInterface::MaybeGetFromContents(web_contents());
  if (tab_interface) {
    auto* helper = tabs::PageContextEligibilityHelper::From(tab_interface);
    if (helper) {
      return helper->IsPageContextEligible() ==
             optimization_guide::PageContextEligibilityStatus::kEligible;
    }
  }
  if (page_context_tracker_) {
    return page_context_tracker_->IsPageContextEligible() ==
           optimization_guide::PageContextEligibilityStatus::kEligible;
  }
  return false;
}

void GlicSelectionObserver::ProcessPendingSelection() {
  is_selecting_ = false;
  is_key_selection_ = false;
  if (!pending_selection_text_.has_value()) {
    return;
  }

  std::u16string selected_text = std::move(*pending_selection_text_);
  ResetPendingSelection();

  UpdateSelectionState(selected_text, /*is_pending_selection=*/true,
                       SelectionSource::kAutomatic);
}

void GlicSelectionObserver::ResetPendingSelection() {
  pending_selection_text_.reset();
}

void GlicSelectionObserver::ProcessInputEvent(
    std::unique_ptr<blink::WebInputEvent> event) {
  if (!IsTextSelectionSharingEnabled() && !has_sent_selection_context_) {
    return;
  }

  shake_trigger_->OnInputEvent(*event);

  switch (event->GetType()) {
    case blink::WebInputEvent::Type::kMouseDown:
    case blink::WebInputEvent::Type::kPointerDown:
    case blink::WebInputEvent::Type::kGestureTapDown:
    case blink::WebInputEvent::Type::kTouchStart: {
      bool is_left_click_or_touch = true;
      if (event->GetType() == blink::WebInputEvent::Type::kMouseDown ||
          event->GetType() == blink::WebInputEvent::Type::kPointerDown) {
        const auto& mouse_event =
            static_cast<const blink::WebMouseEvent&>(*event);
        if (mouse_event.button != blink::WebPointerProperties::Button::kLeft) {
          is_left_click_or_touch = false;
        }
      }

      is_key_selection_ = false;
      bounds_retry_count_ = 0;
      DismissUI(DismissReason::kExternal);

      // Workaround for a bug in Blink: when a user single-clicks directly on
      // top of an existing selection, Blink collapses the selection on MouseUp
      // but fails to send the corresponding OnTextSelectionChanged(empty) IPC.
      // Since any left-click or touch tap invalidates the current static text
      // selection (by either placing the caret, clearing the selection, or
      // initiating a new drag), we preemptively clear the context here to
      // ensure it is not left hanging.
      if (is_left_click_or_touch) {
        is_selecting_ = true;
        ResetPendingSelection();
        if (has_sent_selection_context_) {
          UpdateSelectionState(std::u16string(),
                               /*is_pending_selection=*/false,
                               SelectionSource::kAutomatic);
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
      // Process the selection received so far. If the final selection IPC is
      // delayed, OnTextSelectionChanged will handle it since `is_selecting_`
      // becomes false.
      ProcessPendingSelection();
      break;

    case blink::WebInputEvent::Type::kKeyUp:
      if (is_key_selection_) {
        ProcessPendingSelection();
      }
      break;

    case blink::WebInputEvent::Type::kRawKeyDown:
    case blink::WebInputEvent::Type::kKeyDown: {
      if (is_key_selection_) {
        break;
      }
      DismissUI(DismissReason::kExternal);
      const auto& keyboard_event =
          static_cast<const blink::WebKeyboardEvent&>(*event);
#if BUILDFLAG(IS_MAC)
      int select_all_modifier = blink::WebInputEvent::Modifiers::kMetaKey;
#else
      int select_all_modifier = blink::WebInputEvent::Modifiers::kControlKey;
#endif
      bool is_select_all = (event->GetModifiers() & select_all_modifier) &&
                           keyboard_event.windows_key_code == ui::VKEY_A;
      bool is_shift =
          event->GetModifiers() & blink::WebInputEvent::Modifiers::kShiftKey;
      bool is_navigation_key =
          keyboard_event.windows_key_code == ui::VKEY_LEFT ||
          keyboard_event.windows_key_code == ui::VKEY_RIGHT ||
          keyboard_event.windows_key_code == ui::VKEY_UP ||
          keyboard_event.windows_key_code == ui::VKEY_DOWN ||
          keyboard_event.windows_key_code == ui::VKEY_HOME ||
          keyboard_event.windows_key_code == ui::VKEY_END ||
          keyboard_event.windows_key_code == ui::VKEY_PRIOR ||
          keyboard_event.windows_key_code == ui::VKEY_NEXT;
      if ((is_shift && is_navigation_key) || is_select_all) {
        is_key_selection_ = true;
        is_selecting_ = true;
        if (!last_selected_text_.empty()) {
          pending_selection_text_ = last_selected_text_;
        } else {
          ResetPendingSelection();
        }
      }
      break;
    }

    case blink::WebInputEvent::Type::kGestureScrollBegin:
    case blink::WebInputEvent::Type::kMouseWheel:
      DismissUI(DismissReason::kExternal);
      break;

    default:
      break;
  }
}

void GlicSelectionObserver::OnGlobalPanelShowHide() {
  if (last_selected_text_.empty()) {
    return;
  }

  UpdateSelectionState(last_selected_text_, /*is_pending_selection=*/false,
                       SelectionSource::kAutomatic);
}

void GlicSelectionObserver::CreatePageContextEligibilityAPI(
    std::string account) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(&optimization_guide::PageContextEligibility::Get),
      base::BindOnce(&GlicSelectionObserver::OnPageContextEligibilityAPILoaded,
                     weak_ptr_factory_.GetWeakPtr(), std::move(account)));
}

void GlicSelectionObserver::OnPageContextEligibilityAPILoaded(
    std::string account,
    optimization_guide::PageContextEligibility* page_context_eligibility) {
  if (!page_context_eligibility) {
    return;
  }
  page_context_tracker_ =
      optimization_guide::PageContextEligibilityObserver::Create(
          web_contents(), std::move(account),
          base::BindRepeating(
              [](base::WeakPtr<GlicSelectionObserver> observer,
                 optimization_guide::PageContextEligibilityStatus status) {
                if (observer) {
                  observer->OnPageContextEligibilityChanged(status);
                }
              },
              weak_ptr_factory_.GetWeakPtr()));
}

void GlicSelectionObserver::ResetSelectionState() {
  widget_controller_->OnPrimaryPageChanged();
  // This should close the widget and clear most selection state.
  UpdateSelectionState(u"", /*is_pending_selection=*/false,
                       SelectionSource::kAutomatic);
  // This should clear the rest.
  ResetPendingSelection();
}

}  // namespace glic
