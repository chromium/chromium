// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_selection_observer.h"

#include <functional>
#include <optional>
#include <string_view>

#include "base/containers/flat_set.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "chrome/browser/glic/browser_ui/glic_selection_widget_controller.h"
#include "chrome/browser/glic/host/context/glic_sharing_utils.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_side_panel_coordinator.h"
#include "chrome/browser/glic/public/service/glic_instance_coordinator.h"
#include "chrome/browser/glic/selection/quick_answers_tool.h"
#include "chrome/browser/glic/selection/selection_context_sender.h"
#include "chrome/browser/glic/selection/shake_trigger.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
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

// Returns true if `text` has at least `kMinSelectionLength` non-whitespace
// characters.
bool HasMinSelectionLength(std::u16string_view text) {
  size_t non_whitespace_count = 0;
  for (char16_t c : text) {
    if (!base::IsUnicodeWhitespace(c) &&
        ++non_whitespace_count == kMinSelectionLength) {
      return true;
    }
  }
  return false;
}

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
  last_web_contents_size_ = web_contents->GetSize();
  shake_trigger_ = std::make_unique<ShakeTrigger>(web_contents, *this);

  auto* tab_interface = tabs::TabInterface::MaybeGetFromContents(web_contents);
  if (tab_interface) {
    selection_context_sender_ = SelectionContextSender::From(
        base::PassKey<GlicSelectionObserver>(), tab_interface);
    scoped_unowned_user_data_ =
        std::make_unique<ui::ScopedUnownedUserData<GlicSelectionObserver>>(
            tab_interface->GetUnownedUserDataHost(), *this);
    EnsureSidePanelStateSubscription();
  }

  if (auto* service = GlicKeyedService::Get(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()))) {
    panel_state_subscription_ =
        service->instance_coordinator().AddGlobalShowHideCallback(
            base::BindRepeating(&GlicSelectionObserver::OnGlobalPanelShowHide,
                                weak_ptr_factory_.GetWeakPtr()));
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
  if (!IsTextSelectionSharingEnabled() && !has_sent_selection_context()) {
    return;
  }

  // On unshareable pages (such as chrome:// URLs), automatic selection sharing
  // is disabled. However, if context was explicitly sent (e.g. via the context
  // menu), any deselection must clear that context from the panel.
  if (!IsTabValidForSharing(web_contents())) {
    if (has_sent_selection_context()) {
      UpdateSelectionState(std::u16string(), /*is_pending_selection=*/false,
                           SelectionSource::kAutomatic);
    }
    return;
  }

  // Any selection counts as a wiggle follow-up, including the editable and
  // long ones the inline cue ignores. Hashed, as these can be large.
  if (shake_trigger_->IsCounting()) {
    std::u16string_view follow_up_text =
        base::TrimWhitespace(selected_text, base::TRIM_ALL);
    follow_up_selection_hash_ =
        HasMinSelectionLength(follow_up_text)
            ? std::make_optional(
                  std::hash<std::u16string_view>{}(follow_up_text))
            : std::nullopt;
  }

  if (web_contents()->IsFocusedElementEditable()) {
    selected_text = std::u16string_view();
  }

  bounds_retry_count_ = 0;

  std::u16string_view trimmed_text =
      base::TrimWhitespace(selected_text, base::TRIM_ALL);

  if (trimmed_text.length() > kMaxSelectionLength ||
      !HasMinSelectionLength(trimmed_text)) {
    pending_selection_text_ = std::u16string();
  } else {
    pending_selection_text_ = std::u16string(trimmed_text);
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

void GlicSelectionObserver::UpdateSelectionStateFromSmartSuggestionContextMenu(
    const std::u16string& selected_text) {
  ResetPendingSelection();
  if (last_selected_text_ == selected_text && has_sent_selection_context()) {
    return;
  }
  UpdateSelectionState(selected_text, /*is_pending_selection=*/false,
                       SelectionSource::kAutomatic);
}

bool GlicSelectionObserver::has_sent_selection_context() const {
  auto* sender = GetSelectionContextSender();
  return sender && sender->has_sent_selection_context();
}

SelectionContextSender* GlicSelectionObserver::GetSelectionContextSender()
    const {
  if (selection_context_sender_) {
    return selection_context_sender_;
  }
  auto* tab_interface =
      tabs::TabInterface::MaybeGetFromContents(web_contents());
  return SelectionContextSender::From(base::PassKey<GlicSelectionObserver>(),
                                      tab_interface);
}

void GlicSelectionObserver::OnGlobalPanelShowHide() {
  auto* sender = GetSelectionContextSender();
  if (sender && sender->IsPanelShowing()) {
    CloseSelectionAffordance();
  }
}

void GlicSelectionObserver::DismissUI(DismissReason reason) {
  bounds_retry_count_ = 0;
  ResetPendingSelection();
  widget_controller_->Dismiss(reason);
}

bool GlicSelectionObserver::IsShowingWidgetForTesting() const {
  return widget_controller_->IsShowingWidgetForTesting();
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
  // Ignore notifications where the WebContents size is unchanged (e.g., when
  // `SynchronizeVisualProperties()` fires for non-size changes such as surface
  // ID or screen info updates).
  const gfx::Size current_size = web_contents()->GetSize();
  if (current_size == last_web_contents_size_) {
    return;
  }
  last_web_contents_size_ = current_size;
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
  // Only the shake trigger uses mouse moves. Skip them when it doesn't need
  // them, so that each mouse move doesn't post a task.
  if (event.GetType() == blink::WebInputEvent::Type::kMouseMove &&
      !shake_trigger_->ShouldObserveMouseMoves()) {
    return;
  }
  // If text selection context was previously sent to the panel (e.g. via the
  // "Ask Gemini" context menu), we must still process input events even on
  // unshareable pages (such as chrome:// URLs) so that clicking away can clear
  // the selection chip from the panel.
  if (!IsTabValidForSharing(web_contents()) && !has_sent_selection_context()) {
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

  auto* sender = GetSelectionContextSender();
  if (source == SelectionSource::kContextMenu) {
    if (sender) {
      sender->MarkTextSent(selected_text);
    }
    return;
  }

  if (selected_text.empty()) {
    CloseSelectionAffordance();
    if (sender) {
      sender->SetText(u"");
    }
    last_selection_frame_token_.reset();
    return;
  }

  BrowserWindowInterface* bwi = tab_interface->GetBrowserWindowInterface();
  if (!bwi) {
    if (sender) {
      sender->SetText(selected_text);
    }
    return;
  }

  bool panel_showing = sender && sender->IsPanelShowing();
  bool show_widget =
      is_pending_selection && IsInlineCueEnabled() &&
      IsQuickAnswersEligible() &&
      // Only normal browser windows show the widget, not popups or app windows.
      bwi->GetType() == BrowserWindowInterface::Type::TYPE_NORMAL;

  if (show_widget) {
    ShowSelectionAffordance(selected_text);
  } else if (panel_showing) {
    CloseSelectionAffordance();
  }

  if (sender) {
    sender->SetText(selected_text);
  }
}

bool GlicSelectionObserver::IsInlineCueEnabled() const {
  if (!IsTextSelectionSharingEnabled()) {
    return false;
  }
  return GlicEnabling::IsInlineCueEnabledForProfile(
      Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
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

void GlicSelectionObserver::CloseSelectionAffordance() {
  widget_controller_->Close();
}

void GlicSelectionObserver::ProcessPendingSelection() {
  is_selecting_ = false;
  is_key_selection_ = false;
  if (!pending_selection_text_.has_value()) {
    return;
  }

  std::u16string selected_text = std::move(*pending_selection_text_);
  ResetPendingSelection();
  // Bounds retries and keyboard re-seeds process the same selection again.
  if (follow_up_selection_hash_ &&
      follow_up_selection_hash_ != counted_follow_up_selection_hash_) {
    shake_trigger_->OnFollowUpAction();
  }
  counted_follow_up_selection_hash_ = follow_up_selection_hash_;

  UpdateSelectionState(selected_text, /*is_pending_selection=*/true,
                       SelectionSource::kAutomatic);
}

void GlicSelectionObserver::ResetPendingSelection() {
  pending_selection_text_.reset();
}

void GlicSelectionObserver::ResetFollowUpSelection() {
  follow_up_selection_hash_.reset();
  counted_follow_up_selection_hash_.reset();
}

void GlicSelectionObserver::ProcessInputEvent(
    std::unique_ptr<blink::WebInputEvent> event) {
  if (!IsTextSelectionSharingEnabled() && !has_sent_selection_context()) {
    return;
  }

  EnsureSidePanelStateSubscription();
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
        ResetFollowUpSelection();
        if (has_sent_selection_context()) {
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

void GlicSelectionObserver::EnsureSidePanelStateSubscription() {
  if (side_panel_state_subscription_ || !shake_trigger_->IsCounting()) {
    return;
  }
  auto* tab_interface =
      tabs::TabInterface::MaybeGetFromContents(web_contents());
  auto* coordinator = tab_interface
                          ? GlicSidePanelCoordinator::GetForTab(tab_interface)
                          : nullptr;
  if (coordinator) {
    last_side_panel_state_ = coordinator->state();
    side_panel_state_subscription_ = coordinator->AddStateCallback(
        base::BindRepeating(&GlicSelectionObserver::OnSidePanelStateChanged,
                            weak_ptr_factory_.GetWeakPtr()));
  }
}

void GlicSelectionObserver::OnSidePanelStateChanged(
    GlicSidePanelCoordinator::State state) {
  GlicSidePanelCoordinator::State previous_state = last_side_panel_state_;
  last_side_panel_state_ = state;
  // TODO(b:583899691): Also count region capture while the panel is open.
  if (state == GlicSidePanelCoordinator::State::kShown &&
      previous_state != GlicSidePanelCoordinator::State::kShown &&
      previous_state != GlicSidePanelCoordinator::State::kBackgrounded) {
    shake_trigger_->OnFollowUpAction();
  }
}

void GlicSelectionObserver::ResetSelectionState() {
  widget_controller_->OnPrimaryPageChanged();
  // This should close the widget and clear most selection state.
  UpdateSelectionState(u"", /*is_pending_selection=*/false,
                       SelectionSource::kAutomatic);
  // This should clear the rest.
  ResetPendingSelection();
  ResetFollowUpSelection();
}

bool GlicSelectionObserver::IsQuickAnswersEligible() const {
  if (!base::FeatureList::IsEnabled(
          features::kQuickAnswersSelectionSuggestions)) {
    return true;
  }
  auto* tool = QuickAnswersTool::FromTabWebContents(web_contents());
  return tool && tool->IsEligible();
}

}  // namespace glic
