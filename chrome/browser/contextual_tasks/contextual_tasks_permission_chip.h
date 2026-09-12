// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_PERMISSION_CHIP_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_PERMISSION_CHIP_H_

#include <string>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "chrome/browser/ui/views/permissions/chip/permission_chip_interface.h"
#include "chrome/browser/ui/views/permissions/chip/permission_chip_theme.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_style.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"
#include "components/permissions/permission_actions_history.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

class LocationBar;

namespace contextual_tasks {

class ContextualTasksWebView;

// TODO(crbug.com/559188237): Create a shared base class besides interface for
// ContextualTasksPermissionChip and WebUIPermissionChip to deduplicate shared
// WebUI permission chip implementation details.
class ContextualTasksPermissionChip : public PermissionChipInterface {
 public:
  using WebViewCallback = base::RepeatingCallback<ContextualTasksWebView*()>;
  using AnnounceAlertCallback =
      base::RepeatingCallback<void(const std::u16string&)>;

  explicit ContextualTasksPermissionChip(
      LocationBar* location_bar,
      WebViewCallback web_view_callback,
      ui::ElementIdentifier element_id,
      base::RepeatingClosure update_state_callback = base::DoNothing(),
      AnnounceAlertCallback announce_alert_callback = base::DoNothing());
  ~ContextualTasksPermissionChip() override;

  ContextualTasksPermissionChip(const ContextualTasksPermissionChip&) = delete;
  ContextualTasksPermissionChip& operator=(
      const ContextualTasksPermissionChip&) = delete;

  // PermissionChipInterface:
  void SetVisible(bool visible) override;
  bool GetVisible() const override;
  PermissionChipTheme GetThemeForTesting() const override;
  std::u16string GetTooltipText() const override;
  std::u16string GetTextForTesting() const override;
  bool GetIsRequestForTesting() const override;
  void SetChipIcon(const gfx::VectorIcon& icon) override;
  void SetChipIcon(const gfx::VectorIcon* icon) override;
  void SetMessage(std::u16string message) override;
  void SetTooltipText(const std::u16string& tooltip) override;
  void SetTheme(PermissionChipTheme theme) override;
  void SetUserDecision(permissions::PermissionAction user_decision) override;
  void SetBlockedIconShowing(bool should_show_blocked_icon) override;
  void SetPermissionPromptStyle(PermissionPromptStyle prompt_style) override;

  // Animation methods
  void AnimateCollapse(base::TimeDelta duration) override;
  void AnimateExpand(base::TimeDelta duration) override;
  void AnimateToFit(base::TimeDelta duration) override;
  void ResetAnimation(AnimationState state) override;
  bool IsFullyCollapsed() const override;
  bool IsAnimating() const override;

  // Observers and visibility callbacks
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  [[nodiscard]] base::CallbackListSubscription AddVisibilityCallback(
      base::RepeatingClosure callback) override;

  void SetAccessibilityIgnored(bool is_ignored) override;
  void SetAccessibilityName(const std::u16string& name) override;
  void AnnounceText(const std::u16string& text) override;
  void AnnounceAlert(const std::u16string& text) override;

  bool IsMouseHovered() const override;
  void SetPressedCallback(
      base::RepeatingCallback<void(bool)> callback) override;
  views::BubbleAnchor GetAnchor() override;
  void SetBubbleOwner(BubbleOwnerDelegate* owner) override;
  void ExecuteForTesting() override;
  void EndAnimationForTesting() override;

  // Frontend events routed from WebUI:
  void OnExpandAnimationEnded();
  void OnCollapseAnimationEnded();
  void OnMousePressed();
  void OnClicked(bool is_pointer_interaction);
  void OnMouseEntered();
  void OnMouseExited();

  // Serialization helper
  toolbar_ui_api::mojom::PermissionChipStatePtr GetState() const;

 private:
  void NotifyVisibilityChanged();
  void UpdateState();

  raw_ptr<LocationBar> location_bar_ = nullptr;
  WebViewCallback web_view_callback_;
  ui::ElementIdentifier element_id_;
  base::RepeatingClosure update_state_callback_;
  AnnounceAlertCallback announce_alert_callback_;

  bool is_visible_ = false;
  std::string icon_name_;
  std::u16string message_;
  std::u16string tooltip_;
  PermissionChipTheme theme_ = PermissionChipTheme::kNormalVisibility;
  permissions::PermissionAction user_decision_ =
      permissions::PermissionAction::GRANTED;
  bool should_show_blocked_icon_ = false;
  PermissionPromptStyle prompt_style_ = PermissionPromptStyle::kChip;

  // Declarative target states for the CSS animation engine
  bool is_fully_collapsed_ = true;
  bool should_collapse_ = true;
  bool is_animating_ = false;
  bool is_mouse_hovered_ = false;

  std::u16string accessibility_name_;
  raw_ptr<BubbleOwnerDelegate> bubble_owner_ = nullptr;
  base::RepeatingCallback<void(bool)> pressed_callback_;

  base::ObserverList<
      Observer,
      /*check_empty=*/false,
      base::ObserverListReentrancyPolicy::kAllowReentrancyUntriaged>
      observers_;
  base::RepeatingClosureList visibility_callbacks_;
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_PERMISSION_CHIP_H_
