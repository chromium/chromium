// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/compat_mode/compat_mode_button_controller.h"

#include <string>

#include "ash/components/arc/compat_mode/arc_resize_lock_pref_delegate.h"
#include "ash/components/arc/compat_mode/arc_window_property_util.h"
#include "ash/components/arc/compat_mode/compat_mode_button.h"
#include "ash/components/arc/compat_mode/resize_util.h"
#include "ash/components/arc/vector_icons/vector_icons.h"
#include "ash/frame/non_client_frame_view_ash.h"
#include "ash/game_dashboard/game_dashboard_controller.h"
#include "ash/public/cpp/app_types_util.h"
#include "ash/public/cpp/arc_resize_lock_type.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/ui/frame/default_frame_header.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/vector_icons.h"

namespace arc {

namespace {

const gfx::VectorIcon& GetIcon(const ResizeCompatMode& mode) {
  switch (mode) {
    case ResizeCompatMode::kPhone:
      return chromeos::features::IsJellyEnabled()
                 ? ash::kSystemMenuPhoneIcon
                 : ash::kSystemMenuPhoneLegacyIcon;
    case ResizeCompatMode::kTablet:
      return chromeos::features::IsJellyEnabled()
                 ? ash::kSystemMenuTabletIcon
                 : ash::kSystemMenuTabletLegacyIcon;
    case ResizeCompatMode::kResizable:
      return kResizableIcon;
  }
}

std::u16string GetText(const ResizeCompatMode& mode) {
  switch (mode) {
    case ResizeCompatMode::kPhone:
      return l10n_util::GetStringUTF16(
          IDS_ARC_COMPAT_MODE_RESIZE_TOGGLE_MENU_PHONE);
    case ResizeCompatMode::kTablet:
      return l10n_util::GetStringUTF16(
          IDS_ARC_COMPAT_MODE_RESIZE_TOGGLE_MENU_TABLET);
    case ResizeCompatMode::kResizable:
      return l10n_util::GetStringUTF16(
          IDS_ARC_COMPAT_MODE_RESIZE_TOGGLE_MENU_RESIZABLE);
  }
}

}  // namespace

CompatModeButtonController::CompatModeButtonController() = default;
CompatModeButtonController::~CompatModeButtonController() = default;

void CompatModeButtonController::Update(
    ArcResizeLockPrefDelegate* pref_delegate,
    aura::Window* window) {
  DCHECK(ash::IsArcWindow(window));

  if (ash::GameDashboardController::IsGameWindow(window)) {
    return;
  }
  const auto app_id = GetAppId(window);
  if (!app_id)
    return;
  auto* const frame_header = GetFrameHeader(window);
  // TODO(b/200230343): Replace it with resize lock type.
  const auto resize_lock_state = pref_delegate->GetResizeLockState(*app_id);
  if (resize_lock_state == mojom::ArcResizeLockState::UNDEFINED ||
      resize_lock_state == mojom::ArcResizeLockState::READY) {
    return;
  }
  auto* compat_mode_button = frame_header->GetCenterButton();
  if (!compat_mode_button) {
    // The ownership is transferred implicitly with AddChildView in HeaderView,
    // but ideally we want to explicitly manage the lifecycle of this resource.
    compat_mode_button = new CompatModeButton(
        this,
        base::BindRepeating(&CompatModeButtonController::ToggleResizeToggleMenu,
                            GetWeakPtr(), window, pref_delegate));
    frame_header->SetCenterButton(compat_mode_button);

    UpdateArrowIcon(window, /*widget_visibility=*/false);

    auto* const frame_view = ash::NonClientFrameViewAsh::Get(window);
    // Ideally, we want HeaderView to update properties, but as currently
    // the center button is set to FrameHeader, we need to call this explicitly.
    // |frame_view| can be null in unittest.
    if (frame_view)
      frame_view->GetHeaderView()->UpdateCaptionButtons();
  }

  const auto mode = PredictCurrentMode(window);
  const auto& icon = GetIcon(mode);
  const auto text = GetText(mode);

  compat_mode_button->SetImage(views::CAPTION_BUTTON_ICON_CENTER,
                               views::FrameCaptionButton::Animate::kNo, icon);
  compat_mode_button->SetText(text);
  compat_mode_button->SetAccessibleName(text);

  const auto resize_lock_type = window->GetProperty(ash::kArcResizeLockTypeKey);
  switch (resize_lock_type) {
    case ash::ArcResizeLockType::RESIZE_DISABLED_TOGGLABLE:
    case ash::ArcResizeLockType::RESIZE_ENABLED_TOGGLABLE:
      compat_mode_button->SetEnabled(true);
      break;
    case ash::ArcResizeLockType::RESIZE_DISABLED_NONTOGGLABLE:
      compat_mode_button->SetEnabled(false);
      compat_mode_button->SetTooltipText(l10n_util::GetStringUTF16(
          IDS_ASH_ARC_APP_COMPAT_DISABLED_COMPAT_MODE_BUTTON_TOOLTIP_PHONE));
      break;
    case ash::ArcResizeLockType::NONE:
      // Maximizing an app with RESIZE_ENABLED_TOGGLABLE can lead to this case.
      // Resize lock state shouldn't be updated as the pre-maximized state
      // needs to be restored later.
      break;
  }

  UpdateAshAccelerator(pref_delegate, window);
}

void CompatModeButtonController::OnButtonPressed() {
  visible_when_button_pressed_ =
      resize_toggle_menu_ && resize_toggle_menu_->IsBubbleShown();
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
  if (chromeos::features::IsJellyEnabled()) {
    compat_mode_button->SetSubImage(widget_visibility ? ash::kKsvArrowUpIcon
                                                      : ash::kKsvArrowDownIcon);
  } else {
    compat_mode_button->SetSubImage(views::kMenuDropArrowIcon);
  }
  compat_mode_button->SchedulePaint();
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

void CompatModeButtonController::UpdateAshAccelerator(
    ArcResizeLockPrefDelegate* pref_delegate,
    aura::Window* window) {
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
          window, pref_delegate));
      break;
    case ash::ArcResizeLockType::RESIZE_DISABLED_NONTOGGLABLE:
      frame_view->ClearToggleResizeLockMenuCallback();
      break;
  }
}

void CompatModeButtonController::ToggleResizeToggleMenu(
    aura::Window* window,
    ArcResizeLockPrefDelegate* pref_delegate) {
  if (!window || !ash::IsArcWindow(window))
    return;

  auto* frame_view = ash::NonClientFrameViewAsh::Get(window);
  DCHECK(frame_view);
  const auto* compat_mode_button =
      frame_view->GetHeaderView()->GetFrameHeader()->GetCenterButton();
  if (!compat_mode_button || !compat_mode_button->GetEnabled())
    return;
  if (visible_when_button_pressed_)
    return;
  resize_toggle_menu_.reset();
  resize_toggle_menu_ = std::make_unique<ResizeToggleMenu>(
      base::BindOnce(&CompatModeButtonController::UpdateArrowIcon,
                     base::Unretained(this), window,
                     /*widget_visibility=*/false),
      frame_view->frame(), pref_delegate);
  UpdateArrowIcon(window, /*widget_visibility=*/true);
}

}  // namespace arc
