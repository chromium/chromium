// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/top_shortcuts_view.h"

#include <numeric>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/shutdown_controller_impl.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/unified/collapse_button.h"
#include "ash/system/unified/sign_out_button.h"
#include "ash/system/unified/top_shortcut_button.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/system/unified/user_chooser_detailed_view_controller.h"
#include "ash/system/unified/user_chooser_view.h"
#include "base/numerics/ranges.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view_class_properties.h"
namespace ash {

namespace {

class UserAvatarButton : public views::Button {
 public:
  UserAvatarButton(views::ButtonListener* listener);
  ~UserAvatarButton() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(UserAvatarButton);
};

UserAvatarButton::UserAvatarButton(views::ButtonListener* listener)
    : Button(listener) {
  SetID(VIEW_ID_USER_AVATAR_BUTTON);
  SetLayoutManager(std::make_unique<views::FillLayout>());
  SetBorder(views::CreateEmptyBorder(kUnifiedCircularButtonFocusPadding));
  AddChildView(CreateUserAvatarView(0 /* user_index */));

  SetTooltipText(GetUserItemAccessibleString(0 /* user_index */));
  SetInstallFocusRingOnFocus(true);
  SetFocusForPlatform();

  views::InstallCircleHighlightPathGenerator(this);
}

}  // namespace

TopShortcutButtonContainer::TopShortcutButtonContainer() = default;

TopShortcutButtonContainer::~TopShortcutButtonContainer() = default;

// Buttons are equally spaced by the default value, but the gap will be
// narrowed evenly when the parent view is not large enough.
void TopShortcutButtonContainer::Layout() {
  const gfx::Rect child_area = GetContentsBounds();

  views::View::Views visible_children;
  std::copy_if(children().cbegin(), children().cend(),
               std::back_inserter(visible_children), [](const auto* v) {
                 return v->GetVisible() && (v->GetPreferredSize().width() > 0);
               });
  if (visible_children.empty())
    return;

  const int visible_child_width =
      std::accumulate(visible_children.cbegin(), visible_children.cend(), 0,
                      [](int width, const auto* v) {
                        return width + v->GetPreferredSize().width();
                      });

  int spacing = 0;
  if (visible_children.size() > 1) {
    spacing = (child_area.width() - visible_child_width) /
              (int{visible_children.size()} - 1);
    spacing = base::ClampToRange(spacing, kUnifiedTopShortcutButtonMinSpacing,
                                 kUnifiedTopShortcutButtonDefaultSpacing);
  }

  int x = child_area.x();
  int y = child_area.y() + kUnifiedTopShortcutContainerTopPadding +
          kUnifiedCircularButtonFocusPadding.bottom();
  for (auto* child : visible_children) {
    int child_y = y;
    int width = child->GetPreferredSize().width();
    if (child == user_avatar_button_) {
      x -= kUnifiedCircularButtonFocusPadding.left();
      child_y -= kUnifiedCircularButtonFocusPadding.bottom();
    } else if (child == sign_out_button_) {
      // When there's not enough space, shrink the sign-out button.
      const int remainder = child_area.width() -
                            (int{visible_children.size()} - 1) * spacing -
                            (visible_child_width - width);
      width = base::ClampToRange(width, 0, remainder);
    }

    child->SetBounds(x, child_y, width, child->GetHeightForWidth(width));
    x += width + spacing;

    if (child == user_avatar_button_)
      x -= kUnifiedCircularButtonFocusPadding.right();
  }
}

gfx::Size TopShortcutButtonContainer::CalculatePreferredSize() const {
  int total_horizontal_size = 0;
  int num_visible = 0;
  for (const auto* child : children()) {
    if (!child->GetVisible())
      continue;
    int child_horizontal_size = child->GetPreferredSize().width();
    if (child_horizontal_size == 0)
      continue;
    total_horizontal_size += child_horizontal_size;
    num_visible++;
  }
  int width =
      (num_visible == 0)
          ? 0
          : total_horizontal_size +
                (num_visible - 1) * kUnifiedTopShortcutButtonDefaultSpacing;
  int height = kTrayItemSize + kUnifiedCircularButtonFocusPadding.height() +
               kUnifiedTopShortcutContainerTopPadding;
  return gfx::Size(width, height);
}

const char* TopShortcutButtonContainer::GetClassName() const {
  return "TopShortcutButtonContainer";
}

void TopShortcutButtonContainer::AddUserAvatarButton(
    views::View* user_avatar_button) {
  AddChildView(user_avatar_button);
  user_avatar_button_ = user_avatar_button;
}

void TopShortcutButtonContainer::AddSignOutButton(
    views::View* sign_out_button) {
  AddChildView(sign_out_button);
  sign_out_button_ = sign_out_button;
}

TopShortcutsView::TopShortcutsView(UnifiedSystemTrayController* controller)
    : controller_(controller) {
  DCHECK(controller_);

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, kUnifiedTopShortcutPadding,
      kUnifiedTopShortcutSpacing));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);
  container_ = new TopShortcutButtonContainer();
  AddChildView(container_);

  ash::Shell* shell = Shell::Get();

  bool is_on_login_screen =
      shell->session_controller()->login_status() == LoginStatus::NOT_LOGGED_IN;
  bool can_show_settings = TrayPopupUtils::CanOpenWebUISettings();
  bool can_lock_screen = shell->session_controller()->CanLockScreen();

  if (!is_on_login_screen) {
    user_avatar_button_ = new UserAvatarButton(this);
    user_avatar_button_->SetEnabled(
        UserChooserDetailedViewController::IsUserChooserEnabled());
    container_->AddUserAvatarButton(user_avatar_button_);

    sign_out_button_ = new SignOutButton(this);
    container_->AddSignOutButton(sign_out_button_);
  }

  bool reboot = shell->shutdown_controller()->reboot_on_shutdown();
  power_button_ = new TopShortcutButton(
      this, kUnifiedMenuPowerIcon,
      reboot ? IDS_ASH_STATUS_TRAY_REBOOT : IDS_ASH_STATUS_TRAY_SHUTDOWN);
  power_button_->SetID(VIEW_ID_POWER_BUTTON);
  container_->AddChildView(power_button_);

  if (can_show_settings && can_lock_screen) {
    lock_button_ = new TopShortcutButton(this, kUnifiedMenuLockIcon,
                                         IDS_ASH_STATUS_TRAY_LOCK);
    container_->AddChildView(lock_button_);
  }

  if (can_show_settings) {
    settings_button_ = new TopShortcutButton(this, kUnifiedMenuSettingsIcon,
                                             IDS_ASH_STATUS_TRAY_SETTINGS);
    container_->AddChildView(settings_button_);
  }

  // |collapse_button_| should be right-aligned, so we make the buttons
  // container flex occupying all remaining space.
  layout->SetFlexForView(container_, 1);

  collapse_button_ = new CollapseButton(this);
  AddChildView(collapse_button_);

  OnAccessibilityStatusChanged();

  shell->accessibility_controller()->AddObserver(this);
}

TopShortcutsView::~TopShortcutsView() {
  Shell::Get()->accessibility_controller()->RemoveObserver(this);
}

void TopShortcutsView::SetExpandedAmount(double expanded_amount) {
  collapse_button_->SetExpandedAmount(expanded_amount);
}

void TopShortcutsView::ButtonPressed(views::Button* sender,
                                     const ui::Event& event) {
  if (sender == user_avatar_button_)
    controller_->ShowUserChooserView();
  else if (sender == sign_out_button_)
    controller_->HandleSignOutAction();
  else if (sender == lock_button_)
    controller_->HandleLockAction();
  else if (sender == settings_button_)
    controller_->HandleSettingsAction();
  else if (sender == power_button_)
    controller_->HandlePowerAction();
  else if (sender == collapse_button_)
    controller_->ToggleExpanded();
}

void TopShortcutsView::OnAccessibilityStatusChanged() {
  collapse_button_->SetEnabled(
      !Shell::Get()->accessibility_controller()->spoken_feedback_enabled());
}

const char* TopShortcutsView::GetClassName() const {
  return "TopShortcutsView";
}

}  // namespace ash
