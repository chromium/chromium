// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/top_shortcuts_view.h"

#include <numeric>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/shutdown_controller_impl.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/icon_button.h"
#include "ash/style/pill_button.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/unified/collapse_button.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/system/unified/user_chooser_detailed_view_controller.h"
#include "ash/system/unified/user_chooser_view.h"
#include "ash/system/user/login_status.h"
#include "base/bind.h"
#include "base/cxx17_backports.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/color/color_id.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view_class_properties.h"

namespace ash {

namespace {

class UserAvatarButton : public views::Button {
 public:
  explicit UserAvatarButton(PressedCallback callback);

  UserAvatarButton(const UserAvatarButton&) = delete;
  UserAvatarButton& operator=(const UserAvatarButton&) = delete;

  ~UserAvatarButton() override = default;
};

UserAvatarButton::UserAvatarButton(PressedCallback callback)
    : Button(std::move(callback)) {
  SetID(VIEW_ID_USER_AVATAR_BUTTON);
  SetLayoutManager(std::make_unique<views::FillLayout>());
  SetBorder(views::CreateEmptyBorder(kUnifiedCircularButtonFocusPadding));
  AddChildView(CreateUserAvatarView(0 /* user_index */));

  SetTooltipText(GetUserItemAccessibleString(0 /* user_index */));
  SetInstallFocusRingOnFocus(true);
  views::FocusRing::Get(this)->SetColorId(ui::kColorAshFocusRing);

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
              (static_cast<int>(visible_children.size()) - 1);
    spacing = base::clamp(spacing, kUnifiedTopShortcutButtonMinSpacing,
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
      const int remainder =
          child_area.width() -
          (static_cast<int>(visible_children.size()) - 1) * spacing -
          (visible_child_width - width);
      width = base::clamp(width, 0, std::max(0, remainder));
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

TopShortcutsView::TopShortcutsView(UnifiedSystemTrayController* controller) {
  DCHECK(controller);

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, kUnifiedTopShortcutPadding,
      kUnifiedTopShortcutSpacing));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);
  container_ = new TopShortcutButtonContainer();
  AddChildView(container_);

  Shell* shell = Shell::Get();

  bool is_on_login_screen =
      shell->session_controller()->login_status() == LoginStatus::NOT_LOGGED_IN;
  bool can_show_settings = TrayPopupUtils::CanOpenWebUISettings();
  bool can_lock_screen = shell->session_controller()->CanLockScreen();

  if (!is_on_login_screen) {
    user_avatar_button_ = new UserAvatarButton(
        base::BindRepeating(&UnifiedSystemTrayController::ShowUserChooserView,
                            base::Unretained(controller)));
    user_avatar_button_->SetEnabled(
        UserChooserDetailedViewController::IsUserChooserEnabled());
    container_->AddUserAvatarButton(user_avatar_button_);

    sign_out_button_ = new PillButton(
        base::BindRepeating(&UnifiedSystemTrayController::HandleSignOutAction,
                            base::Unretained(controller)),
        user::GetLocalizedSignOutStringForStatus(
            Shell::Get()->session_controller()->login_status(),
            /*multiline=*/false),
        PillButton::Type::kIconless,
        /*icon=*/nullptr);
    container_->AddSignOutButton(sign_out_button_);
  }

  bool reboot = shell->shutdown_controller()->reboot_on_shutdown();

  power_button_ = new IconButton(
      base::BindRepeating(&UnifiedSystemTrayController::HandlePowerAction,
                          base::Unretained(controller)),
      IconButton::Type::kSmall, &kUnifiedMenuPowerIcon,
      reboot ? IDS_ASH_STATUS_TRAY_REBOOT : IDS_ASH_STATUS_TRAY_SHUTDOWN);
  power_button_->SetID(VIEW_ID_POWER_BUTTON);
  container_->AddChildView(power_button_);

  if (can_show_settings && can_lock_screen) {
    lock_button_ = new IconButton(
        base::BindRepeating(&UnifiedSystemTrayController::HandleLockAction,
                            base::Unretained(controller)),
        IconButton::Type::kSmall, &kUnifiedMenuLockIcon,
        IDS_ASH_STATUS_TRAY_LOCK);
    container_->AddChildView(lock_button_);
  }

  if (can_show_settings) {
    settings_button_ = new IconButton(
        base::BindRepeating(&UnifiedSystemTrayController::HandleSettingsAction,
                            base::Unretained(controller)),
        IconButton::Type::kSmall, &vector_icons::kSettingsOutlineIcon,
        IDS_ASH_STATUS_TRAY_SETTINGS);
    settings_button_->SetID(VIEW_ID_SETTINGS_BUTTON_VIEW);
    container_->AddChildView(settings_button_);
    local_state_pref_change_registrar_.Init(Shell::Get()->local_state());
    local_state_pref_change_registrar_.Add(
        prefs::kOsSettingsEnabled,
        base::BindRepeating(&TopShortcutsView::UpdateSettingsButtonState,
                            base::Unretained(this)));
    UpdateSettingsButtonState();
  }

  // |collapse_button_| should be right-aligned, so we make the buttons
  // container flex occupying all remaining space.
  layout->SetFlexForView(container_, 1);

  auto* collapse_button_container =
      AddChildView(std::make_unique<views::View>());
  collapse_button_ =
      collapse_button_container->AddChildView(std::make_unique<CollapseButton>(
          base::BindRepeating(&UnifiedSystemTrayController::ToggleExpanded,
                              base::Unretained(controller))));
  const gfx::Size collapse_button_size = collapse_button_->GetPreferredSize();
  collapse_button_container->SetPreferredSize(
      gfx::Size(collapse_button_size.width(),
                collapse_button_size.height() + kUnifiedTopShortcutSpacing));
  collapse_button_->SetBoundsRect(gfx::Rect(
      gfx::Point(0, kUnifiedTopShortcutSpacing), collapse_button_size));
}

// static
void TopShortcutsView::RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kOsSettingsEnabled, true);
}

void TopShortcutsView::SetExpandedAmount(double expanded_amount) {
  collapse_button_->SetExpandedAmount(expanded_amount);
}

const char* TopShortcutsView::GetClassName() const {
  return "TopShortcutsView";
}

void TopShortcutsView::UpdateSettingsButtonState() {
  PrefService* const local_state = Shell::Get()->local_state();
  const bool settings_icon_enabled =
      local_state->GetBoolean(prefs::kOsSettingsEnabled);

  settings_button_->SetState(settings_icon_enabled
                                 ? views::Button::STATE_NORMAL
                                 : views::Button::STATE_DISABLED);
}

}  // namespace ash
