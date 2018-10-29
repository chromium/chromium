// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/message_center_button_bar.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/message_center/ash_message_center_lock_screen_controller.h"
#include "ash/system/message_center/message_center_style.h"
#include "ash/system/message_center/message_center_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/text_constants.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/button/menu_button_listener.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/painter.h"

using message_center::MessageCenter;

namespace ash {

namespace {

constexpr SkColor kTextColor = SkColorSetARGB(0xFF, 0x0, 0x0, 0x0);
constexpr SkColor kButtonSeparatorColor = SkColorSetARGB(0x1F, 0x0, 0x0, 0x0);
constexpr int kTextFontSizeDelta = 2;
constexpr int kSeparatorHeight = 24;
constexpr gfx::Insets kSeparatorPadding(12, 0, 12, 0);
constexpr gfx::Insets kButtonBarBorder(4, 18, 4, 0);

// A ToggleImageButton that implements system menu style ink drop animation.
class MessageCenterButton : public views::ToggleImageButton {
 public:
  MessageCenterButton(views::ButtonListener* listener)
      : ToggleImageButton(listener) {
    SetBorder(
        views::CreateEmptyBorder(message_center_style::kActionIconPadding));
    set_animate_on_state_change(true);

    TrayPopupUtils::ConfigureTrayPopupButton(this);
  }

  std::unique_ptr<views::InkDrop> CreateInkDrop() override {
    return TrayPopupUtils::CreateInkDrop(this);
  }

  std::unique_ptr<views::InkDropRipple> CreateInkDropRipple() const override {
    return TrayPopupUtils::CreateInkDropRipple(
        TrayPopupInkDropStyle::HOST_CENTERED, this,
        GetInkDropCenterBasedOnLastEvent());
  }

  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override {
    return TrayPopupUtils::CreateInkDropHighlight(
        TrayPopupInkDropStyle::HOST_CENTERED, this);
  }

  std::unique_ptr<views::InkDropMask> CreateInkDropMask() const override {
    return TrayPopupUtils::CreateInkDropMask(
        TrayPopupInkDropStyle::HOST_CENTERED, this);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MessageCenterButton);
};

views::Separator* CreateVerticalSeparator() {
  views::Separator* separator = new views::Separator;
  separator->SetPreferredHeight(kSeparatorHeight);
  separator->SetColor(kButtonSeparatorColor);
  separator->SetBorder(views::CreateEmptyBorder(kSeparatorPadding));
  return separator;
}

}  // namespace

// MessageCenterButtonBar /////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
MessageCenterButtonBar::MessageCenterButtonBar(
    MessageCenterView* message_center_view,
    MessageCenter* message_center,
    bool locked)
    : message_center_view_(message_center_view),
      message_center_(message_center),
      notification_label_(nullptr),
      button_container_(nullptr),
      close_all_button_(nullptr),
      quiet_mode_button_(nullptr),
      settings_button_(nullptr) {
  SetPaintToLayer();
  SetBackground(
      views::CreateSolidBackground(message_center_style::kBackgroundColor));
  SetBorder(views::CreateEmptyBorder(kButtonBarBorder));

  notification_label_ = new views::Label(
      GetTitle(!locked || AshMessageCenterLockScreenController::IsEnabled()));
  notification_label_->SetAutoColorReadabilityEnabled(false);
  notification_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  notification_label_->SetEnabledColor(kTextColor);
  // "Roboto-Medium, 14sp" is specified in the mock.
  notification_label_->SetFontList(gfx::FontList().Derive(
      kTextFontSizeDelta, gfx::Font::NORMAL, gfx::Font::Weight::MEDIUM));
  AddChildView(notification_label_);

  button_container_ = new views::View;
  button_container_->SetPaintToLayer();
  button_container_->SetBackground(
      views::CreateSolidBackground(message_center_style::kBackgroundColor));
  button_container_->SetLayoutManager(
      std::make_unique<views::BoxLayout>(views::BoxLayout::kHorizontal));
  close_all_button_ = new MessageCenterButton(this);
  close_all_button_->SetImage(
      views::Button::STATE_NORMAL,
      gfx::CreateVectorIcon(kNotificationCenterClearAllIcon, kMenuIconSize,
                            kMenuIconColor));
  close_all_button_->SetImage(
      views::Button::STATE_DISABLED,
      gfx::CreateVectorIcon(kNotificationCenterClearAllIcon, kMenuIconSize,
                            kMenuIconColorDisabled));
  close_all_button_->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_ASH_MESSAGE_CENTER_CLEAR_ALL_BUTTON_TOOLTIP));
  button_container_->AddChildView(close_all_button_);
  separator_1_ = CreateVerticalSeparator();
  button_container_->AddChildView(separator_1_);

  quiet_mode_button_ = new MessageCenterButton(this);
  quiet_mode_button_->SetImage(
      views::Button::STATE_NORMAL,
      gfx::CreateVectorIcon(kNotificationCenterDoNotDisturbOffIcon,
                            kMenuIconSize, kMenuIconColorDisabled));
  gfx::ImageSkia quiet_mode_toggle_icon = gfx::CreateVectorIcon(
      kNotificationCenterDoNotDisturbOnIcon, kMenuIconSize, kMenuIconColor);
  quiet_mode_button_->SetToggledImage(views::Button::STATE_NORMAL,
                                      &quiet_mode_toggle_icon);
  quiet_mode_button_->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_ASH_MESSAGE_CENTER_QUIET_MODE_BUTTON_TOOLTIP));
  SetQuietModeState(message_center->IsQuietMode());
  button_container_->AddChildView(quiet_mode_button_);
  separator_2_ = CreateVerticalSeparator();
  button_container_->AddChildView(separator_2_);

  settings_button_ = new MessageCenterButton(this);
  settings_button_->SetImage(
      views::Button::STATE_NORMAL,
      gfx::CreateVectorIcon(kNotificationCenterSettingsIcon, kMenuIconSize,
                            kMenuIconColor));
  settings_button_->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_ASH_MESSAGE_CENTER_SETTINGS_BUTTON_TOOLTIP));
  button_container_->AddChildView(settings_button_);

  collapse_button_ = new MessageCenterButton(this);
  collapse_button_->SetVisible(false);
  collapse_button_->SetBackground(
      views::CreateSolidBackground(message_center_style::kBackgroundColor));
  collapse_button_->SetPaintToLayer();
  collapse_button_->SetImage(
      views::Button::STATE_NORMAL,
      gfx::CreateVectorIcon(kNotificationCenterCollapseIcon, kMenuIconSize,
                            kMenuIconColor));
  collapse_button_->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_ASH_MESSAGE_CENTER_COLLAPSE_BUTTON_TOOLTIP));
  AddChildView(collapse_button_);

  AddChildView(button_container_);

  SetCloseAllButtonEnabled(true);
  SetBackArrowVisible(false);
}

MessageCenterButtonBar::~MessageCenterButtonBar() = default;

void MessageCenterButtonBar::SetSettingsAndQuietModeButtonsEnabled(
    bool enabled) {
  settings_button_->SetEnabled(enabled);
  quiet_mode_button_->SetEnabled(enabled);
}

void MessageCenterButtonBar::SetCloseAllButtonEnabled(bool enabled) {
  if (close_all_button_)
    close_all_button_->SetEnabled(enabled);
}

views::Button* MessageCenterButtonBar::GetCloseAllButtonForTest() const {
  return close_all_button_;
}

views::Button* MessageCenterButtonBar::GetQuietModeButtonForTest() const {
  return quiet_mode_button_;
}

views::Button* MessageCenterButtonBar::GetSettingsButtonForTest() const {
  return settings_button_;
}

views::Button* MessageCenterButtonBar::GetCollapseButtonForTest() const {
  return collapse_button_;
}

void MessageCenterButtonBar::SetBackArrowVisible(bool visible) {
  if (collapse_button_visible_ == visible)
    return;

  collapse_button_visible_ = visible;

  collapse_button_->SetVisible(true);
  button_container_->SetVisible(true);

  collapse_button_->layer()->SetOpacity(visible ? 0.0 : 1.0);
  button_container_->layer()->SetOpacity(visible ? 1.0 : 0.0);

  ui::ScopedLayerAnimationSettings collapse_settings(
      collapse_button_->layer()->GetAnimator());
  collapse_settings.AddObserver(this);
  collapse_settings.SetTweenType(gfx::Tween::EASE_IN_OUT);
  collapse_settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(
      message_center_style::kSettingsTransitionDurationMs));

  ui::ScopedLayerAnimationSettings container_settings(
      button_container_->layer()->GetAnimator());
  container_settings.SetTweenType(gfx::Tween::EASE_IN_OUT);
  container_settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(
      message_center_style::kSettingsTransitionDurationMs));

  collapse_button_->layer()->SetOpacity(visible ? 1.0 : 0.0);
  button_container_->layer()->SetOpacity(visible ? 0.0 : 1.0);
}

void MessageCenterButtonBar::OnImplicitAnimationsCompleted() {
  bool settings_focused =
      GetFocusManager() &&
      (GetFocusManager()->GetFocusedView() == collapse_button_ ||
       GetFocusManager()->GetFocusedView() == settings_button_);

  if (settings_focused) {
    if (collapse_button_visible_)
      collapse_button_->RequestFocus();
    else
      settings_button_->RequestFocus();
  }

  collapse_button_->SetVisible(collapse_button_visible_);
  button_container_->SetVisible(!collapse_button_visible_);
}

void MessageCenterButtonBar::SetIsLocked(bool locked) {
  SetButtonsVisible(locked);
  UpdateLabel(!locked || AshMessageCenterLockScreenController::IsEnabled());
}

base::string16 MessageCenterButtonBar::GetTitle(
    bool message_center_visible) const {
  return message_center_visible
             ? l10n_util::GetStringUTF16(IDS_ASH_MESSAGE_CENTER_FOOTER_TITLE)
             : l10n_util::GetStringUTF16(
                   IDS_ASH_MESSAGE_CENTER_FOOTER_LOCKSCREEN);
}

void MessageCenterButtonBar::UpdateLabel(bool message_center_visible) {
  notification_label_->SetText(GetTitle(message_center_visible));
  // On lock screen button bar label contains hint for user to unlock device to
  // view notifications. Making it focusable will invoke ChromeVox spoken
  // feedback when shown.
  notification_label_->SetFocusBehavior(
      message_center_visible ? FocusBehavior::ALWAYS : FocusBehavior::NEVER);
}

void MessageCenterButtonBar::SetButtonsVisible(bool locked) {
  bool message_center_visible =
      !locked || AshMessageCenterLockScreenController::IsEnabled();
  if (close_all_button_)
    close_all_button_->SetVisible(message_center_visible);
  separator_1_->SetVisible(message_center_visible);
  quiet_mode_button_->SetVisible(message_center_visible);
  separator_2_->SetVisible(!locked);
  settings_button_->SetVisible(!locked);

  Layout();
}

void MessageCenterButtonBar::SetQuietModeState(bool is_quiet_mode) {
  quiet_mode_button_->SetToggled(is_quiet_mode);
}

void MessageCenterButtonBar::ChildVisibilityChanged(views::View* child) {
  InvalidateLayout();
}

void MessageCenterButtonBar::Layout() {
  gfx::Rect child_area = GetContentsBounds();

  notification_label_->SetBounds(
      child_area.x(), child_area.y(),
      notification_label_->GetPreferredSize().width(), child_area.height());

  int button_container_width = button_container_->GetPreferredSize().width();
  button_container_->SetBounds(child_area.right() - button_container_width,
                               child_area.y(), button_container_width,
                               child_area.height());

  int collapse_button_width = collapse_button_->GetPreferredSize().width();
  collapse_button_->SetBounds(child_area.right() - collapse_button_width,
                              child_area.y(), collapse_button_width,
                              child_area.height());
}

gfx::Size MessageCenterButtonBar::CalculatePreferredSize() const {
  int preferred_height =
      std::max(button_container_->GetPreferredSize().height(),
               collapse_button_->GetPreferredSize().height()) +
      GetInsets().height();
  return gfx::Size(0, preferred_height);
}

void MessageCenterButtonBar::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kDialog;
  node_data->SetName(notification_label_->text());
}

void MessageCenterButtonBar::ButtonPressed(views::Button* sender,
                                           const ui::Event& event) {
  if (sender == close_all_button_) {
    message_center_view()->ClearAllClosableNotifications();
  } else if (sender == settings_button_) {
    // In order to implement a bit tricky animation specified in UX mock, it
    // calls ACTION_TRIGGERED of |collapse_button_| on |settings_button_| click.
    // ACTION_TRIGGERED of |settings_button_| was already called by
    // has_ink_drop_action_on_click().
    // |settings_button_| and |collapse_button_| are in the same position,
    // and SetSettingsVisible() below triggers cross-fading between them.
    collapse_button_->AnimateInkDrop(views::InkDropState::ACTION_TRIGGERED,
                                     nullptr);
    message_center_view()->SetSettingsVisible(true);
  } else if (sender == collapse_button_) {
    // Same as above.
    settings_button_->AnimateInkDrop(views::InkDropState::ACTION_TRIGGERED,
                                     nullptr);
    message_center_view()->SetSettingsVisible(false);
  } else if (sender == quiet_mode_button_) {
    if (message_center()->IsQuietMode())
      message_center()->SetQuietMode(false);
    else
      message_center()->EnterQuietModeWithExpire(base::TimeDelta::FromDays(1));
  } else {
    NOTREACHED();
  }
}

}  // namespace ash
