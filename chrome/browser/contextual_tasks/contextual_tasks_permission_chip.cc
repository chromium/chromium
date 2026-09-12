// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_permission_chip.h"

#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_web_view.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/webui/tracked_element/tracked_element_handler.h"
#include "ui/webui/tracked_element/tracked_element_handler_document_singleton.h"

namespace contextual_tasks {

namespace {

toolbar_ui_api::mojom::PermissionChipTheme GetMojoTheme(
    PermissionChipTheme theme) {
  switch (theme) {
    case PermissionChipTheme::kNormalVisibility:
      return toolbar_ui_api::mojom::PermissionChipTheme::kNormalVisibility;
    case PermissionChipTheme::kLowVisibility:
      return toolbar_ui_api::mojom::PermissionChipTheme::kLowVisibility;
    case PermissionChipTheme::kInUseActivityIndicator:
      return toolbar_ui_api::mojom::PermissionChipTheme::
          kInUseActivityIndicator;
    case PermissionChipTheme::kBlockedActivityIndicator:
      return toolbar_ui_api::mojom::PermissionChipTheme::
          kBlockedActivityIndicator;
    case PermissionChipTheme::kOnSystemBlockedActivityIndicator:
      return toolbar_ui_api::mojom::PermissionChipTheme::
          kOnSystemBlockedActivityIndicator;
  }
  NOTREACHED();
}

toolbar_ui_api::mojom::PermissionPromptStyle GetMojoPromptStyle(
    PermissionPromptStyle style) {
  switch (style) {
    case PermissionPromptStyle::kBubbleOnly:
      return toolbar_ui_api::mojom::PermissionPromptStyle::kBubbleOnly;
    case PermissionPromptStyle::kChip:
      return toolbar_ui_api::mojom::PermissionPromptStyle::kChip;
    case PermissionPromptStyle::kLocationBarRightIcon:
      return toolbar_ui_api::mojom::PermissionPromptStyle::
          kLocationBarRightIcon;
    case PermissionPromptStyle::kQuietChip:
      return toolbar_ui_api::mojom::PermissionPromptStyle::kQuietChip;
  }
  NOTREACHED();
}

toolbar_ui_api::mojom::PermissionAction GetMojoPermissionAction(
    permissions::PermissionAction action) {
  switch (action) {
    case permissions::PermissionAction::GRANTED:
      return toolbar_ui_api::mojom::PermissionAction::kGranted;
    case permissions::PermissionAction::DENIED:
      return toolbar_ui_api::mojom::PermissionAction::kDenied;
    case permissions::PermissionAction::DISMISSED:
      return toolbar_ui_api::mojom::PermissionAction::kDismissed;
    case permissions::PermissionAction::IGNORED:
      return toolbar_ui_api::mojom::PermissionAction::kIgnored;
    case permissions::PermissionAction::REVOKED:
      return toolbar_ui_api::mojom::PermissionAction::kRevoked;
    case permissions::PermissionAction::GRANTED_ONCE:
      return toolbar_ui_api::mojom::PermissionAction::kGrantedOnce;
    case permissions::PermissionAction::NUM:
      return toolbar_ui_api::mojom::PermissionAction::kUnspecified;
  }
  NOTREACHED();
}

}  // namespace

ContextualTasksPermissionChip::ContextualTasksPermissionChip(
    LocationBar* location_bar,
    WebViewCallback web_view_callback,
    ui::ElementIdentifier element_id,
    base::RepeatingClosure update_state_callback,
    AnnounceAlertCallback announce_alert_callback)
    : location_bar_(location_bar),
      web_view_callback_(std::move(web_view_callback)),
      element_id_(element_id),
      update_state_callback_(std::move(update_state_callback)),
      announce_alert_callback_(std::move(announce_alert_callback)) {}

ContextualTasksPermissionChip::~ContextualTasksPermissionChip() = default;

void ContextualTasksPermissionChip::SetVisible(bool visible) {
  if (is_visible_ == visible) {
    return;
  }
  is_visible_ = visible;
  NotifyVisibilityChanged();
  UpdateState();
}

bool ContextualTasksPermissionChip::GetVisible() const {
  return is_visible_;
}

PermissionChipTheme ContextualTasksPermissionChip::GetThemeForTesting() const {
  return theme_;
}

std::u16string ContextualTasksPermissionChip::GetTooltipText() const {
  return tooltip_;
}

std::u16string ContextualTasksPermissionChip::GetTextForTesting() const {
  return message_;
}

bool ContextualTasksPermissionChip::GetIsRequestForTesting() const {
  switch (theme_) {
    case PermissionChipTheme::kNormalVisibility:
    case PermissionChipTheme::kLowVisibility:
      return true;
    case PermissionChipTheme::kBlockedActivityIndicator:
    case PermissionChipTheme::kOnSystemBlockedActivityIndicator:
    case PermissionChipTheme::kInUseActivityIndicator:
      return false;
  }
}

void ContextualTasksPermissionChip::SetChipIcon(const gfx::VectorIcon& icon) {
  icon_name_ = icon.name;
  UpdateState();
}

void ContextualTasksPermissionChip::SetChipIcon(const gfx::VectorIcon* icon) {
  if (icon) {
    icon_name_ = icon->name;
    UpdateState();
  }
}

void ContextualTasksPermissionChip::SetMessage(std::u16string message) {
  message_ = std::move(message);
  UpdateState();
}

void ContextualTasksPermissionChip::SetTooltipText(
    const std::u16string& tooltip) {
  tooltip_ = tooltip;
  UpdateState();
}

void ContextualTasksPermissionChip::SetTheme(PermissionChipTheme theme) {
  theme_ = theme;
  UpdateState();
}

void ContextualTasksPermissionChip::SetUserDecision(
    permissions::PermissionAction user_decision) {
  user_decision_ = user_decision;
  UpdateState();
}

void ContextualTasksPermissionChip::SetBlockedIconShowing(
    bool should_show_blocked_icon) {
  should_show_blocked_icon_ = should_show_blocked_icon;
  UpdateState();
}

void ContextualTasksPermissionChip::SetPermissionPromptStyle(
    PermissionPromptStyle prompt_style) {
  prompt_style_ = prompt_style;
  UpdateState();
}

void ContextualTasksPermissionChip::AnimateCollapse(base::TimeDelta duration) {
  // Mirror Native Views' gfx::SlideAnimation::BeginAnimating() behavior:
  // if the animation is already targeting this state, do nothing. This prevents
  // us from hanging indefinitely on `is_animating_` since the frontend won't
  // fire an IPC for a redundant DOM state.
  if (should_collapse_) {
    return;
  }
  is_animating_ = true;
  should_collapse_ = true;
  UpdateState();
}

void ContextualTasksPermissionChip::AnimateExpand(base::TimeDelta duration) {
  // Mirror Native Views' gfx::SlideAnimation::BeginAnimating() behavior:
  // if the animation is already targeting this state, do nothing. This prevents
  // us from hanging indefinitely on `is_animating_` since the frontend won't
  // fire an IPC for a redundant DOM state.
  if (!should_collapse_) {
    return;
  }
  is_animating_ = true;
  is_fully_collapsed_ = false;
  should_collapse_ = false;
  UpdateState();
}

void ContextualTasksPermissionChip::AnimateToFit(base::TimeDelta duration) {
  AnimateExpand(duration);
}

void ContextualTasksPermissionChip::ResetAnimation(AnimationState state) {
  bool was_animating = is_animating_;
  is_animating_ = false;

  // Instantly snap the C++ backend to the requested state.
  is_fully_collapsed_ = (state == AnimationState::kCollapsed);

  // Synchronize the Mojo target state with the C++ backend state.
  // Note: This triggers a DOM update on the frontend. We must not rely on the
  // frontend's subsequent asynchronous IPC to notify observers because C++
  // callers (e.g., ChipController) expect ResetAnimation to be fully
  // synchronous. The `is_animating_ = false` assignment above acts as a
  // firewall, ensuring we safely drop the WebUI's late, redundant IPC.
  should_collapse_ = is_fully_collapsed_;

  // In Native Views, gfx::Animation::Reset() calls Stop(), which synchronously
  // fires AnimationEnded() if the animation was currently running. We must
  // mirror that synchronous callback here.
  if (was_animating) {
    if (is_fully_collapsed_) {
      observers_.Notify(&Observer::OnCollapseAnimationEnded);
    } else {
      observers_.Notify(&Observer::OnExpandAnimationEnded);
    }
  }

  UpdateState();
}

bool ContextualTasksPermissionChip::IsFullyCollapsed() const {
  return is_fully_collapsed_;
}

bool ContextualTasksPermissionChip::IsAnimating() const {
  return is_animating_;
}

void ContextualTasksPermissionChip::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ContextualTasksPermissionChip::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

base::CallbackListSubscription
ContextualTasksPermissionChip::AddVisibilityCallback(
    base::RepeatingClosure callback) {
  return visibility_callbacks_.Add(std::move(callback));
}

void ContextualTasksPermissionChip::SetAccessibilityIgnored(bool is_ignored) {
  // No-op for WebUI. When the chip is hidden in WebUI, it is given the
  // `visibility: hidden` CSS property, meaning it naturally drops out of the
  // accessibility tree without needing explicit aria-hidden flags.
}

void ContextualTasksPermissionChip::SetAccessibilityName(
    const std::u16string& name) {
  accessibility_name_ = name;
  UpdateState();
}

// For accessibility announcements.
void ContextualTasksPermissionChip::AnnounceText(const std::u16string& text) {
  AnnounceAlert(text);
}

void ContextualTasksPermissionChip::AnnounceAlert(const std::u16string& text) {
  if (location_bar_) {
    location_bar_->AnnounceAlert(text);
  }
  if (announce_alert_callback_) {
    announce_alert_callback_.Run(text);
  }
}

bool ContextualTasksPermissionChip::IsMouseHovered() const {
  return is_mouse_hovered_;
}

void ContextualTasksPermissionChip::SetPressedCallback(
    base::RepeatingCallback<void(bool)> callback) {
  pressed_callback_ = std::move(callback);
}

views::BubbleAnchor ContextualTasksPermissionChip::GetAnchor() {
  ContextualTasksWebView* web_view =
      web_view_callback_ ? web_view_callback_.Run() : nullptr;

  // TODO(crbug.com/558978384): Rethink where the permission bubble should be
  // anchored in the side panel. In the original design, it was anchored to the
  // Super G button, but that button is hidden when a permission chip is shown.
  // Instead, attempt to anchor to the WebUI permission chip element or fall
  // back to the toolbar (kContextualTasksWebUIToolbarElementId) or native web
  // view.

  // Get web contents from location bar, falling back to web_view if needed.
  content::WebContents* webui_contents =
      location_bar_ ? location_bar_->GetWebContents() : nullptr;

  if (webui_contents && webui_contents->GetPrimaryMainFrame()) {
    auto handler = ui::TrackedElementHandlerDocumentSingleton::GetOrCreate(
        webui_contents->GetPrimaryMainFrame());
    if (handler) {
      ui::TrackedElement* element =
          ui::ElementTracker::GetElementTracker()->GetFirstMatchingElement(
              element_id_, handler->context());
      if (!element) {
        element =
            ui::ElementTracker::GetElementTracker()->GetFirstMatchingElement(
                kContextualTasksWebUIToolbarElementId, handler->context());
      }
      if (element) {
        return views::BubbleAnchor(element);
      }
    }
  }

  // Fallback anchor to prevent crashes during asynchronous race conditions
  // (e.g. before the WebUI element has finished registering over Mojo).
  if (web_view && web_view->toolbar_web_view()) {
    return views::BubbleAnchor(web_view->toolbar_web_view());
  }
  if (web_view) {
    return views::BubbleAnchor(web_view);
  }
  return views::BubbleAnchor();
}

void ContextualTasksPermissionChip::SetBubbleOwner(BubbleOwnerDelegate* owner) {
  bubble_owner_ = owner;
}

void ContextualTasksPermissionChip::ExecuteForTesting() {
  if (pressed_callback_) {
    pressed_callback_.Run(/*is_pointer_interaction=*/false);
  }
}

void ContextualTasksPermissionChip::EndAnimationForTesting() {
  ResetAnimation(AnimationState::kCollapsed);
}

void ContextualTasksPermissionChip::OnExpandAnimationEnded() {
  // Ignore blind IPCs sent by the WebUI frontend after a forced synchronous
  // snap triggered by ResetAnimation().
  if (!is_animating_) {
    return;
  }
  // Ignore stale IPCs if a new animation or reset was triggered before the
  // frontend finished processing the previous one.
  if (should_collapse_) {
    return;
  }
  is_animating_ = false;
  is_fully_collapsed_ = false;
  AnnounceAlert(message_);
  observers_.Notify(&Observer::OnExpandAnimationEnded);
}

void ContextualTasksPermissionChip::OnCollapseAnimationEnded() {
  // Ignore blind IPCs sent by the WebUI frontend after a forced synchronous
  // snap triggered by ResetAnimation().
  if (!is_animating_) {
    return;
  }
  // Ignore stale IPCs if a new animation or reset was triggered before the
  // frontend finished processing the previous one.
  if (!should_collapse_) {
    return;
  }
  is_animating_ = false;
  is_fully_collapsed_ = true;
  observers_.Notify(&Observer::OnCollapseAnimationEnded);
}

void ContextualTasksPermissionChip::OnMousePressed() {
  observers_.Notify(&Observer::OnMousePressed);
}

void ContextualTasksPermissionChip::OnClicked(bool is_pointer_interaction) {
  if (pressed_callback_) {
    pressed_callback_.Run(is_pointer_interaction);
  }
}

void ContextualTasksPermissionChip::OnMouseEntered() {
  is_mouse_hovered_ = true;
  if (bubble_owner_ &&
      (bubble_owner_->IsBubbleShowing() || bubble_owner_->IsAnimating())) {
    return;
  }
  if (bubble_owner_) {
    bubble_owner_->RestartTimersOnMouseHover();
  }
}

void ContextualTasksPermissionChip::OnMouseExited() {
  is_mouse_hovered_ = false;
}

toolbar_ui_api::mojom::PermissionChipStatePtr
ContextualTasksPermissionChip::GetState() const {
  auto state = toolbar_ui_api::mojom::PermissionChipState::New();
  state->is_visible = is_visible_;
  state->icon_name = icon_name_;
  state->message = message_;
  state->tooltip = tooltip_;
  state->theme = GetMojoTheme(theme_);
  state->user_decision = GetMojoPermissionAction(user_decision_);
  state->should_show_blocked_icon = should_show_blocked_icon_;
  state->prompt_style = GetMojoPromptStyle(prompt_style_);
  // In a declarative WebUI architecture, the Mojo state represents the target
  // state that triggers the browser's CSS animation engine. We serialize
  // `should_collapse_` (the target state) rather than `is_fully_collapsed_`
  // (the actual C++ state), because sending the target state is what commands
  // the frontend to begin its CSS transition.
  state->is_fully_collapsed = should_collapse_;
  state->accessibility_name = accessibility_name_;
  return state;
}

void ContextualTasksPermissionChip::NotifyVisibilityChanged() {
  observers_.Notify(&Observer::OnChipVisibilityChanged, is_visible_);
  visibility_callbacks_.Notify();
}

void ContextualTasksPermissionChip::UpdateState() {
  if (location_bar_) {
    location_bar_->OnChanged();
  }
  if (update_state_callback_) {
    update_state_callback_.Run();
  }
}

}  // namespace contextual_tasks
