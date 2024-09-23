// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/compat_mode/compat_mode_button_controller.h"

#include "ash/components/arc/compat_mode/arc_resize_lock_pref_delegate.h"
#include "ash/components/arc/compat_mode/arc_window_property_util.h"
#include "ash/components/arc/compat_mode/compat_mode_button.h"
#include "ash/frame/non_client_frame_view_ash.h"
#include "ash/game_dashboard/game_dashboard_controller.h"
#include "ash/public/cpp/app_types_util.h"
#include "ash/public/cpp/arc_compat_mode_util.h"
#include "ash/public/cpp/arc_resize_lock_type.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "base/functional/bind.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/ui/frame/default_frame_header.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/vector_icons.h"

namespace arc {

CompatModeButtonController::ButtonState::ButtonState() = default;

CompatModeButtonController::ButtonState::ButtonState(bool enable) {
  this->enable = enable;
}

CompatModeButtonController::ButtonState::ButtonState(
    bool enable,
    const std::u16string& tooltip_text)
    : CompatModeButtonController::ButtonState(enable) {
  this->tooltip_text = tooltip_text;
}

CompatModeButtonController::ButtonState::ButtonState(const ButtonState& other) =
    default;
CompatModeButtonController::ButtonState::~ButtonState() = default;

CompatModeButtonController::CompatModeButtonController() = default;
CompatModeButtonController::~CompatModeButtonController() = default;

void CompatModeButtonController::Update(aura::Window* window) {
  DCHECK(ash::IsArcWindow(window));
  DCHECK(pref_delegate_);

  const auto app_id = GetAppId(window);
  if (!app_id)
    return;
  auto* const frame_header = GetFrameHeader(window);
  // TODO(b/200230343): Replace it with resize lock type.
  const auto resize_lock_state = pref_delegate_->GetResizeLockState(*app_id);
  if (resize_lock_state == mojom::ArcResizeLockState::UNDEFINED ||
      resize_lock_state == mojom::ArcResizeLockState::READY) {
    return;
  }

  // Update the accelerator, for all windows.
  UpdateAshAccelerator(window);

  // Don't show the `CompatModeButton` for game windows.
  if (ash::GameDashboardController::IsGameWindow(window)) {
    return;
  }

  auto* compat_mode_button = frame_header->GetCenterButton();
  if (!compat_mode_button) {
    // The ownership is transferred implicitly with AddChildView in HeaderView,
    // but ideally we want to explicitly manage the lifecycle of this resource.
    compat_mode_button = new CompatModeButton(
        this,
        base::BindRepeating(&CompatModeButtonController::ToggleResizeToggleMenu,
                            GetWeakPtr(), window));
    frame_header->SetCenterButton(compat_mode_button);

    UpdateArrowIcon(window, /*widget_visibility=*/false);

    auto* const frame_view = ash::NonClientFrameViewAsh::Get(window);
    // Ideally, we want HeaderView to update properties, but as currently
    // the center button is set to FrameHeader, we need to call this explicitly.
    // |frame_view| can be null in unittest.
    if (frame_view)
      frame_view->GetHeaderView()->UpdateCaptionButtons();
  }

  const auto mode = ash::compat_mode_util::PredictCurrentMode(window);
  const auto text = ash::compat_mode_util::GetText(mode);

  compat_mode_button->SetImage(views::CAPTION_BUTTON_ICON_CENTER,
                               views::FrameCaptionButton::Animate::kNo,
                               ash::compat_mode_util::GetIcon(mode));
  compat_mode_button->SetText(text);
  compat_mode_button->GetViewAccessibility().SetName(text);

  if (auto button_state = GetButtonState(window)) {
    compat_mode_button->SetEnabled(button_state->enable);
    if (button_state->tooltip_text) {
      compat_mode_button->SetTooltipText(button_state->tooltip_text.value());
    }
  }
}

void CompatModeButtonController::OnButtonPressed() {
  visible_when_button_pressed_ =
      resize_toggle_menu_ && resize_toggle_menu_->IsBubbleShown();
}

void CompatModeButtonController::ClearPrefDelegate() {
  pref_delegate_ = nullptr;
}

void CompatModeButtonController::SetPrefDelegate(
    ArcResizeLockPrefDelegate* pref_delegate) {
  CHECK(!pref_delegate_);
  pref_delegate_ = pref_delegate;
}

std::optional<CompatModeButtonController::ButtonState>
CompatModeButtonController::GetButtonState(const aura::Window* window) const {
  const auto resize_lock_type = window->GetProperty(ash::kArcResizeLockTypeKey);
  switch (resize_lock_type) {
    case ash::ArcResizeLockType::RESIZE_DISABLED_TOGGLABLE:
    case ash::ArcResizeLockType::RESIZE_ENABLED_TOGGLABLE:
      return ButtonState{/*enable=*/true};
    case ash::ArcResizeLockType::RESIZE_DISABLED_NONTOGGLABLE:
      return ButtonState{
          /*enable=*/false,
          l10n_util::GetStringUTF16(
              IDS_ASH_ARC_APP_COMPAT_DISABLED_COMPAT_MODE_BUTTON_TOOLTIP_PHONE)};
    case ash::ArcResizeLockType::NONE:
      // Maximizing an app with RESIZE_ENABLED_TOGGLABLE can lead to this case.
      // Resize lock state shouldn't be updated as the pre-maximized state
      // needs to be restored later.
      return std::nullopt;
  }
}

void CompatModeButtonController::UpdateArrowIcon(aura::Window* window,
                                                 bool widget_visibility) {
  auto* const frame_view = ash::NonClientFrameViewAsh::Get(window);
  // |frame_view| can be null in unittest.
  if (!frame_view) {
    return;
  }

  auto* const compat_mode_button =
      frame_view->GetHeaderView()->GetFrameHeader()->GetCenterButton();
  compat_mode_button->SetSubImage(widget_visibility ? ash::kKsvArrowUpIcon
                                                    : ash::kKsvArrowDownIcon);
  compat_mode_button->SchedulePaint();
}

void CompatModeButtonController::ShowResizeToggleMenu(
    aura::Window* window,
    base::OnceClosure on_bubble_widget_closing_callback) {
  DCHECK(window) << "Invalid window. Unable to display resize toggle menu.";
  DCHECK(ash::IsArcWindow(window))
      << "Cannot display resize toggle menu on a non-ARC window.";

  auto* frame_view = ash::NonClientFrameViewAsh::Get(window);
  DCHECK(frame_view)
      << "Invalid frame view. Unable to display resize toggle menu.";
  resize_toggle_menu_ = std::make_unique<ResizeToggleMenu>(
      std::move(on_bubble_widget_closing_callback), frame_view->frame(),
      pref_delegate_);
}

base::WeakPtr<CompatModeButtonController>
CompatModeButtonController::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

chromeos::FrameHeader* CompatModeButtonController::GetFrameHeader(
    aura::Window* window) {
  auto* const frame_view = ash::NonClientFrameViewAsh::Get(window);
  return frame_view->GetHeaderView()->GetFrameHeader();
}

void CompatModeButtonController::UpdateAshAccelerator(aura::Window* window) {
  auto* const frame_view = ash::NonClientFrameViewAsh::Get(window);
  // |frame_view| can be null in unittest.
  if (!frame_view)
    return;

  const auto resize_lock_type = window->GetProperty(ash::kArcResizeLockTypeKey);
  switch (resize_lock_type) {
    case ash::ArcResizeLockType::RESIZE_DISABLED_TOGGLABLE:
    // TODO(b/200230343): Call NOTREACHED() once the client has shifted to
    // the new protocol.
    case ash::ArcResizeLockType::NONE:
    case ash::ArcResizeLockType::RESIZE_ENABLED_TOGGLABLE:
      frame_view->SetToggleResizeLockMenuCallback(base::BindRepeating(
          &CompatModeButtonController::ToggleResizeToggleMenu, GetWeakPtr(),
          window));
      break;
    case ash::ArcResizeLockType::RESIZE_DISABLED_NONTOGGLABLE:
      frame_view->ClearToggleResizeLockMenuCallback();
      break;
  }
}

void CompatModeButtonController::ToggleResizeToggleMenu(aura::Window* window) {
  if (!window || !ash::IsArcWindow(window)) {
    return;
  }
  DCHECK(pref_delegate_);

  auto* frame_view = ash::NonClientFrameViewAsh::Get(window);
  DCHECK(frame_view);
  const auto* compat_mode_button =
      frame_view->GetHeaderView()->GetFrameHeader()->GetCenterButton();
  if (!compat_mode_button || !compat_mode_button->GetEnabled())
    return;
  if (visible_when_button_pressed_)
    return;
  ShowResizeToggleMenu(
      window, base::BindOnce(&CompatModeButtonController::UpdateArrowIcon,
                             base::Unretained(this), window,
                             /*widget_visibility=*/false));
  UpdateArrowIcon(window, /*widget_visibility=*/true);
}

}  // namespace arc
