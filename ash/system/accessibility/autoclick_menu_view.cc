// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/autoclick_menu_view.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/top_shortcut_button.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Constants for panel sizing, positioning and coloring.
const int kPanelPositionButtonSize = 36;
const int kPanelPositionButtonPadding = 14;
const int kSeparatorHeight = 16;
const SkColor kAutoclickMenuButtonIconColorActive = SkColorSetRGB(32, 33, 36);

}  // namespace

// A button used in the Automatic clicks on-screen menu. The button is
// togglable.
class AutoclickMenuButton : public TopShortcutButton {
 public:
  AutoclickMenuButton(views::ButtonListener* listener,
                      const gfx::VectorIcon& icon,
                      int accessible_name_id,
                      int size = kTrayItemSize,
                      bool draw_highlight = true)
      : TopShortcutButton(listener, accessible_name_id),
        icon_(&icon),
        size_(size),
        draw_highlight_(draw_highlight) {
    EnableCanvasFlippingForRTLUI(false);
    SetPreferredSize(gfx::Size(size_, size_));
    UpdateImage();
  }

  ~AutoclickMenuButton() override = default;

  // views::Button:
  const char* GetClassName() const override { return "AutoclickMenuButton"; }

  // views::ImageButton:
  std::unique_ptr<views::InkDropMask> CreateInkDropMask() const override {
    gfx::Rect bounds = GetContentsBounds();
    return std::make_unique<views::CircleInkDropMask>(
        size(), bounds.CenterPoint(), bounds.width() / 2);
  }

  // Set the vector icon shown in a circle.
  void SetVectorIcon(const gfx::VectorIcon& icon) {
    icon_ = &icon;
    UpdateImage();
  }

  // Change the toggle state.
  void SetToggled(bool toggled) {
    toggled_ = toggled;
    UpdateImage();
    SchedulePaint();
  }

  bool IsToggled() { return toggled_; }

  // TopShortcutButton:
  void PaintButtonContents(gfx::Canvas* canvas) override {
    if (draw_highlight_) {
      gfx::Rect rect(GetContentsBounds());
      cc::PaintFlags flags;
      flags.setAntiAlias(true);
      flags.setColor(AshColorProvider::Get()->GetControlsLayerColor(
          toggled_
              ? AshColorProvider::ControlsLayerType::kActiveControlBackground
              : AshColorProvider::ControlsLayerType::kInactiveControlBackground,
          AshColorProvider::AshColorMode::kDark));
      flags.setStyle(cc::PaintFlags::kFill_Style);
      canvas->DrawCircle(gfx::PointF(rect.CenterPoint()), size_ / 2, flags);
    }

    views::ImageButton::PaintButtonContents(canvas);
  }

  gfx::Size CalculatePreferredSize() const override {
    return gfx::Size(size_, size_);
  }

  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    if (!GetEnabled())
      return;
    TopShortcutButton::GetAccessibleNodeData(node_data);
    node_data->role = ax::mojom::Role::kToggleButton;
    node_data->SetCheckedState(toggled_ ? ax::mojom::CheckedState::kTrue
                                        : ax::mojom::CheckedState::kFalse);
  }

  void SetId(AutoclickMenuView::ButtonId id) {
    views::View::SetID(static_cast<int>(id));
  }

 private:
  void UpdateImage() {
    const SkColor icon_color = AshColorProvider::Get()->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kIconPrimary,
        AshColorProvider::AshColorMode::kDark);
    SetImage(views::Button::STATE_NORMAL,
             gfx::CreateVectorIcon(
                 *icon_,
                 toggled_ ? kAutoclickMenuButtonIconColorActive : icon_color));
    SetImage(views::Button::STATE_DISABLED,
             gfx::CreateVectorIcon(
                 *icon_,
                 toggled_ ? kAutoclickMenuButtonIconColorActive : icon_color));
  }

  const gfx::VectorIcon* icon_;
  // True if the button is currently toggled.
  bool toggled_ = false;
  int size_;
  const bool draw_highlight_;

  DISALLOW_COPY_AND_ASSIGN(AutoclickMenuButton);
};

// ------ AutoclickMenuBubbleView  ------ //

AutoclickMenuBubbleView::AutoclickMenuBubbleView(
    TrayBubbleView::InitParams init_params)
    : TrayBubbleView(init_params) {}

AutoclickMenuBubbleView::~AutoclickMenuBubbleView() {}

bool AutoclickMenuBubbleView::IsAnchoredToStatusArea() const {
  return false;
}

const char* AutoclickMenuBubbleView::GetClassName() const {
  return "AutoclickMenuBubbleView";
}

// ------ AutoclickMenuView  ------ //

AutoclickMenuView::AutoclickMenuView(AutoclickEventType type,
                                     AutoclickMenuPosition position)
    : left_click_button_(
          new AutoclickMenuButton(this,
                                  kAutoclickLeftClickIcon,
                                  IDS_ASH_AUTOCLICK_OPTION_LEFT_CLICK)),
      right_click_button_(
          new AutoclickMenuButton(this,
                                  kAutoclickRightClickIcon,
                                  IDS_ASH_AUTOCLICK_OPTION_RIGHT_CLICK)),
      double_click_button_(
          new AutoclickMenuButton(this,
                                  kAutoclickDoubleClickIcon,
                                  IDS_ASH_AUTOCLICK_OPTION_DOUBLE_CLICK)),
      drag_button_(
          new AutoclickMenuButton(this,
                                  kAutoclickDragIcon,
                                  IDS_ASH_AUTOCLICK_OPTION_DRAG_AND_DROP)),
      scroll_button_(new AutoclickMenuButton(this,
                                             kAutoclickScrollIcon,
                                             IDS_ASH_AUTOCLICK_OPTION_SCROLL)),
      pause_button_(
          new AutoclickMenuButton(this,
                                  kAutoclickPauseIcon,
                                  IDS_ASH_AUTOCLICK_OPTION_NO_ACTION)),
      position_button_(
          new AutoclickMenuButton(this,
                                  kAutoclickPositionBottomLeftIcon,
                                  IDS_ASH_AUTOCLICK_OPTION_CHANGE_POSITION,
                                  kPanelPositionButtonSize,
                                  false /* no highlight */)) {
  // Set view IDs for testing.
  left_click_button_->SetId(ButtonId::kLeftClick);
  right_click_button_->SetId(ButtonId::kRightClick);
  double_click_button_->SetId(ButtonId::kDoubleClick);
  drag_button_->SetId(ButtonId::kDragAndDrop);
  pause_button_->SetId(ButtonId::kPause);
  position_button_->SetId(ButtonId::kPosition);
  if (scroll_button_)
    scroll_button_->SetId(ButtonId::kScroll);

  std::unique_ptr<views::BoxLayout> layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(), 0);
  layout->set_cross_axis_alignment(views::BoxLayout::CrossAxisAlignment::kEnd);
  SetLayoutManager(std::move(layout));

  // The action control buttons all have the same spacing.
  views::View* action_button_container = new views::View();
  action_button_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, kUnifiedMenuItemPadding,
      kUnifiedTopShortcutSpacing));
  action_button_container->AddChildView(left_click_button_);
  action_button_container->AddChildView(right_click_button_);
  action_button_container->AddChildView(double_click_button_);
  action_button_container->AddChildView(drag_button_);
  if (scroll_button_)
    action_button_container->AddChildView(scroll_button_);
  action_button_container->AddChildView(pause_button_);
  AddChildView(action_button_container);

  views::Separator* separator = new views::Separator();
  separator->SetColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kSeparator,
      AshColorProvider::AshColorMode::kDark));
  separator->SetPreferredHeight(kSeparatorHeight);
  int total_height = kUnifiedTopShortcutSpacing * 2 + kTrayItemSize;
  int separator_spacing = (total_height - kSeparatorHeight) / 2;
  separator->SetBorder(views::CreateEmptyBorder(
      separator_spacing - kUnifiedTopShortcutSpacing, 0, separator_spacing, 0));
  AddChildView(separator);

  views::View* position_button_container = new views::View();
  position_button_container->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          gfx::Insets(0, kPanelPositionButtonPadding,
                      kPanelPositionButtonPadding, kPanelPositionButtonPadding),
          kPanelPositionButtonPadding));
  position_button_container->AddChildView(position_button_);
  AddChildView(position_button_container);

  UpdateEventType(type);
  UpdatePosition(position);
}

void AutoclickMenuView::UpdateEventType(AutoclickEventType type) {
  left_click_button_->SetToggled(type == AutoclickEventType::kLeftClick);
  right_click_button_->SetToggled(type == AutoclickEventType::kRightClick);
  double_click_button_->SetToggled(type == AutoclickEventType::kDoubleClick);
  drag_button_->SetToggled(type == AutoclickEventType::kDragAndDrop);
  if (scroll_button_)
    scroll_button_->SetToggled(type == AutoclickEventType::kScroll);
  pause_button_->SetToggled(type == AutoclickEventType::kNoAction);
  if (type != AutoclickEventType::kNoAction)
    event_type_ = type;
}

void AutoclickMenuView::UpdatePosition(AutoclickMenuPosition position) {
  switch (position) {
    case AutoclickMenuPosition::kBottomRight:
      position_button_->SetVectorIcon(kAutoclickPositionBottomRightIcon);
      return;
    case AutoclickMenuPosition::kBottomLeft:
      position_button_->SetVectorIcon(kAutoclickPositionBottomLeftIcon);
      return;
    case AutoclickMenuPosition::kTopLeft:
      position_button_->SetVectorIcon(kAutoclickPositionTopLeftIcon);
      return;
    case AutoclickMenuPosition::kTopRight:
      position_button_->SetVectorIcon(kAutoclickPositionTopRightIcon);
      return;
    case AutoclickMenuPosition::kSystemDefault:
      position_button_->SetVectorIcon(base::i18n::IsRTL()
                                          ? kAutoclickPositionBottomLeftIcon
                                          : kAutoclickPositionBottomRightIcon);
      return;
  }
}

void AutoclickMenuView::ButtonPressed(views::Button* sender,
                                      const ui::Event& event) {
  if (sender == position_button_) {
    AutoclickMenuPosition new_position;
    // Rotate clockwise throughout the screen positions.
    switch (
        Shell::Get()->accessibility_controller()->GetAutoclickMenuPosition()) {
      case AutoclickMenuPosition::kBottomRight:
        new_position = AutoclickMenuPosition::kBottomLeft;
        break;
      case AutoclickMenuPosition::kBottomLeft:
        new_position = AutoclickMenuPosition::kTopLeft;
        break;
      case AutoclickMenuPosition::kTopLeft:
        new_position = AutoclickMenuPosition::kTopRight;
        break;
      case AutoclickMenuPosition::kTopRight:
        new_position = AutoclickMenuPosition::kBottomRight;
        break;
      case AutoclickMenuPosition::kSystemDefault:
        new_position = base::i18n::IsRTL() ? AutoclickMenuPosition::kTopLeft
                                           : AutoclickMenuPosition::kBottomLeft;
        break;
    }
    Shell::Get()->accessibility_controller()->SetAutoclickMenuPosition(
        new_position);
    base::RecordAction(base::UserMetricsAction(
        "Accessibility.CrosAutoclick.TrayMenu.ChangePosition"));
    return;
  }
  AutoclickEventType type;
  if (sender == left_click_button_) {
    type = AutoclickEventType::kLeftClick;
  } else if (sender == right_click_button_) {
    type = AutoclickEventType::kRightClick;
  } else if (sender == double_click_button_) {
    type = AutoclickEventType::kDoubleClick;
  } else if (sender == drag_button_) {
    type = AutoclickEventType::kDragAndDrop;
  } else if (sender == scroll_button_) {
    type = AutoclickEventType::kScroll;
  } else if (sender == pause_button_) {
    // If the pause button was already selected, tapping it again turns off
    // pause and returns to the previous type.
    if (pause_button_->IsToggled())
      type = event_type_;
    else
      type = AutoclickEventType::kNoAction;
  } else {
    return;
  }

  Shell::Get()->accessibility_controller()->SetAutoclickEventType(type);
  UMA_HISTOGRAM_ENUMERATION("Accessibility.CrosAutoclick.TrayMenu.ChangeAction",
                            type);
}

const char* AutoclickMenuView::GetClassName() const {
  return "AutoclickMenuView";
}

}  // namespace ash
