// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/top_shortcuts_view.h"

#include <cstddef>
#include <memory>
#include <numeric>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/quick_settings_catalogs.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/shutdown_controller_impl.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/style/pill_button.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/unified/collapse_button.h"
#include "ash/system/unified/quick_settings_metrics_util.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/system/unified/user_chooser_detailed_view_controller.h"
#include "ash/system/unified/user_chooser_view.h"
#include "ash/system/user/login_status.h"
#include "base/cxx17_backports.h"
#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/color/color_id.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

namespace {

class UserAvatarButton : public views::Button {
 public:
  explicit UserAvatarButton(PressedCallback callback)
      : Button(std::move(callback)) {
    SetLayoutManager(std::make_unique<views::FillLayout>());
    SetBorder(views::CreateEmptyBorder(kUnifiedCircularButtonFocusPadding));
    AddChildView(CreateUserAvatarView(0 /* user_index */));
    SetTooltipText(GetUserItemAccessibleString(0 /* user_index */));
    SetInstallFocusRingOnFocus(true);
    views::FocusRing::Get(this)->SetColorId(ui::kColorAshFocusRing);

    views::InstallCircleHighlightPathGenerator(this);
  }

  UserAvatarButton(const UserAvatarButton&) = delete;
  UserAvatarButton& operator=(const UserAvatarButton&) = delete;
  ~UserAvatarButton() override = default;
};

}  // namespace

TopShortcutButtonContainer::TopShortcutButtonContainer() = default;

TopShortcutButtonContainer::~TopShortcutButtonContainer() = default;

// Buttons are equally spaced by the default value, but the gap will be
// narrowed evenly when the parent view is not large enough.
void TopShortcutButtonContainer::Layout() {
  const gfx::Rect child_area = GetContentsBounds();

  views::View::Views visible_children;
  base::ranges::copy_if(
      children(), std::back_inserter(visible_children), [](const auto* v) {
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

views::View* TopShortcutButtonContainer::AddUserAvatarButton(
    std::unique_ptr<views::View> user_avatar_button) {
  user_avatar_button_ = AddChildView(std::move(user_avatar_button));
  return user_avatar_button_;
}

views::Button* TopShortcutButtonContainer::AddSignOutButton(
    std::unique_ptr<views::Button> sign_out_button) {
  sign_out_button_ = AddChildView(std::move(sign_out_button));
  return sign_out_button_;
}

TopShortcutsView::TopShortcutsView(UnifiedSystemTrayController* controller) {
  DCHECK(controller);

#if DCHECK_IS_ON()
  // Only need it for `DCHECK` in `OnChildViewAdded`.
  AddObserver(this);
#endif  // DCHECK_IS_ON()

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, kUnifiedTopShortcutPadding,
      kUnifiedTopShortcutSpacing));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);
  auto button_container = std::make_unique<TopShortcutButtonContainer>();

  Shell* shell = Shell::Get();

  bool is_on_login_screen =
      shell->session_controller()->login_status() == LoginStatus::NOT_LOGGED_IN;
  bool can_show_settings = TrayPopupUtils::CanOpenWebUISettings();
  bool can_lock_screen = shell->session_controller()->CanLockScreen();

  if (!is_on_login_screen) {
    user_avatar_button_ = button_container->AddUserAvatarButton(
        std::make_unique<UserAvatarButton>(base::BindRepeating(
            [](UnifiedSystemTrayController* controller) {
              quick_settings_metrics_util::RecordQsButtonActivated(
                  QsButtonCatalogName::kAvatarButton);
              controller->ShowUserChooserView();
            },
            base::Unretained(controller))));
    user_avatar_button_->SetEnabled(
        UserChooserDetailedViewController::IsUserChooserEnabled());
    user_avatar_button_->SetID(VIEW_ID_QS_USER_AVATAR_BUTTON);

    sign_out_button_ =
        button_container->AddSignOutButton(std::make_unique<PillButton>(
            base::BindRepeating(
                [](UnifiedSystemTrayController* controller) {
                  quick_settings_metrics_util::RecordQsButtonActivated(
                      QsButtonCatalogName::kSignOutButton);
                  controller->HandleSignOutAction();
                },
                base::Unretained(controller)),
            user::GetLocalizedSignOutStringForStatus(
                Shell::Get()->session_controller()->login_status(),
                /*multiline=*/false),
            PillButton::Type::kDefaultWithoutIcon,
            /*icon=*/nullptr));
    sign_out_button_->SetID(VIEW_ID_QS_SIGN_OUT_BUTTON);
  }
  bool reboot = shell->shutdown_controller()->reboot_on_shutdown();
  power_button_ = button_container->AddChildView(std::make_unique<IconButton>(
      base::BindRepeating(
          [](UnifiedSystemTrayController* controller) {
            quick_settings_metrics_util::RecordQsButtonActivated(
                QsButtonCatalogName::kPowerButton);
            controller->HandlePowerAction();
          },
          base::Unretained(controller)),
      IconButton::Type::kMedium, &kUnifiedMenuPowerIcon,
      reboot ? IDS_ASH_STATUS_TRAY_REBOOT : IDS_ASH_STATUS_TRAY_SHUTDOWN));
  power_button_->SetID(VIEW_ID_QS_POWER_BUTTON);

  if (can_show_settings && can_lock_screen) {
    lock_button_ = button_container->AddChildView(std::make_unique<IconButton>(
        base::BindRepeating(
            [](UnifiedSystemTrayController* controller) {
              quick_settings_metrics_util::RecordQsButtonActivated(
                  QsButtonCatalogName::kLockButton);
              controller->HandleLockAction();
            },
            base::Unretained(controller)),
        IconButton::Type::kMedium, &kUnifiedMenuLockIcon,
        IDS_ASH_STATUS_TRAY_LOCK));
    lock_button_->SetID(VIEW_ID_QS_LOCK_BUTTON);
  }

  if (can_show_settings) {
    settings_button_ =
        button_container->AddChildView(std::make_unique<IconButton>(
            base::BindRepeating(
                [](UnifiedSystemTrayController* controller) {
                  quick_settings_metrics_util::RecordQsButtonActivated(
                      QsButtonCatalogName::kSettingsButton);
                  controller->HandleSettingsAction();
                },
                base::Unretained(controller)),
            IconButton::Type::kMedium, &vector_icons::kSettingsOutlineIcon,
            IDS_ASH_STATUS_TRAY_SETTINGS));
    settings_button_->SetID(VIEW_ID_QS_SETTINGS_BUTTON);

    local_state_pref_change_registrar_.Init(Shell::Get()->local_state());
    local_state_pref_change_registrar_.Add(
        prefs::kOsSettingsEnabled,
        base::BindRepeating(&TopShortcutsView::UpdateSettingsButtonState,
                            base::Unretained(this)));
    UpdateSettingsButtonState();
  }

  container_ = AddChildView(std::move(button_container));

  // |collapse_button_| should be right-aligned, so we make the buttons
  // container flex occupying all remaining space.
  layout->SetFlexForView(container_, 1);

  if (features::IsQsRevampEnabled())
    return;

  auto collapse_button_container = std::make_unique<views::View>();
  collapse_button_ = collapse_button_container->AddChildView(
      std::make_unique<CollapseButton>(base::BindRepeating(
          [](UnifiedSystemTrayController* controller) {
            quick_settings_metrics_util::RecordQsButtonActivated(
                QsButtonCatalogName::kCollapseButton);
            controller->ToggleExpanded();
          },
          base::Unretained(controller))));
  collapse_button_->SetID(VIEW_ID_QS_COLLAPSE_BUTTON);
  const gfx::Size collapse_button_size = collapse_button_->GetPreferredSize();
  collapse_button_container->SetPreferredSize(
      gfx::Size(collapse_button_size.width(),
                collapse_button_size.height() + kUnifiedTopShortcutSpacing));
  collapse_button_->SetBoundsRect(gfx::Rect(
      gfx::Point(0, kUnifiedTopShortcutSpacing), collapse_button_size));

  AddChildView(std::move(collapse_button_container));
}

TopShortcutsView::~TopShortcutsView() {
#if DCHECK_IS_ON()
  // Only need it for `DCHECK` in `OnChildViewAdded`.
  RemoveObserver(this);
#endif  // DCHECK_IS_ON()
}

// static
void TopShortcutsView::RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kOsSettingsEnabled, true);
}

void TopShortcutsView::SetExpandedAmount(double expanded_amount) {
  if (features::IsQsRevampEnabled())
    return;
  collapse_button_->SetExpandedAmount(expanded_amount);
}

const char* TopShortcutsView::GetClassName() const {
  return "TopShortcutsView";
}

void TopShortcutsView::OnChildViewAdded(View* observed_view, View* child) {
  if (observed_view != this)
    return;

  if (child->children().empty()) {
    DCHECK(child->GetID() >= VIEW_ID_QS_MIN && child->GetID() <= VIEW_ID_QS_MAX)
        << "All buttons directly added to this view must have a view ID with  "
           "VIEW_ID_QS_*, and record a metric using QsButtonCatalogName (see "
           "other usages of the catalog names for an example)";
    return;
  }

  for (View* button : child->children()) {
    DCHECK(button->GetID() >= VIEW_ID_QS_MIN &&
           button->GetID() <= VIEW_ID_QS_MAX)
        << "All buttons directly added to each container must have a view ID "
           "with VIEW_ID_QS_*, and record a metric using QsButtonCatalogName "
           "(see other usages of the catalog names for an example)";
  }
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
