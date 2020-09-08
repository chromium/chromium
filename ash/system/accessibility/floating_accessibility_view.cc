// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/floating_accessibility_view.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/system_tray.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/accessibility/dictation_button_tray.h"
#include "ash/system/accessibility/floating_menu_button.h"
#include "ash/system/accessibility/select_to_speak_tray.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/virtual_keyboard/virtual_keyboard_tray.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// These constants are defined in DIP.
constexpr int kPanelPositionButtonPadding = 14;
constexpr int kPanelPositionButtonSize = 36;
constexpr int kSeparatorHeight = 16;

// The view that hides itself if all of its children are not visible.
class DynamicRowView : public views::View {
 public:
  DynamicRowView() = default;

 protected:
  // views::View:
  void ChildVisibilityChanged(views::View* child) override {
    bool any_visible = false;
    for (auto* view : children()) {
      any_visible |= view->GetVisible();
    }
    SetVisible(any_visible);
  }
};

std::unique_ptr<views::Separator> CreateSeparator() {
  auto separator = std::make_unique<views::Separator>();
  separator->SetColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kSeparatorColor));
  separator->SetPreferredHeight(kSeparatorHeight);
  int total_height = kUnifiedTopShortcutSpacing * 2 + kTrayItemSize;
  int separator_spacing = (total_height - kSeparatorHeight) / 2;
  separator->SetBorder(views::CreateEmptyBorder(
      separator_spacing - kUnifiedTopShortcutSpacing, 0, separator_spacing, 0));
  return separator;
}

std::unique_ptr<views::View> CreateButtonRowContainer(int padding) {
  auto button_container = std::make_unique<DynamicRowView>();
  button_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets(0, padding, padding, padding), padding));
  return button_container;
}

std::string GetDescriptionForMovedToPosition(FloatingMenuPosition position) {
  switch (position) {
    case FloatingMenuPosition::kBottomRight:
      return l10n_util::GetStringUTF8(
          IDS_ASH_FLOATING_ACCESSIBILITY_MAIN_MENU_MOVED_BOTTOM_RIGHT);
    case FloatingMenuPosition::kBottomLeft:
      return l10n_util::GetStringUTF8(
          IDS_ASH_FLOATING_ACCESSIBILITY_MAIN_MENU_MOVED_BOTTOM_LEFT);
    case FloatingMenuPosition::kTopLeft:
      return l10n_util::GetStringUTF8(
          IDS_ASH_FLOATING_ACCESSIBILITY_MAIN_MENU_MOVED_TOP_LEFT);
    case FloatingMenuPosition::kTopRight:
      return l10n_util::GetStringUTF8(
          IDS_ASH_FLOATING_ACCESSIBILITY_MAIN_MENU_MOVED_TOP_RIGHT);
    case FloatingMenuPosition::kSystemDefault:
      NOTREACHED();
      return std::string();
  }
}

}  // namespace

FloatingAccessibilityBubbleView::FloatingAccessibilityBubbleView(
    const TrayBubbleView::InitParams& init_params)
    : TrayBubbleView(init_params) {
  // Intercept ESC keypresses.
  AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
}

FloatingAccessibilityBubbleView::~FloatingAccessibilityBubbleView() = default;

bool FloatingAccessibilityBubbleView::IsAnchoredToStatusArea() const {
  return false;
}

bool FloatingAccessibilityBubbleView::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  DCHECK_EQ(accelerator.key_code(), ui::VKEY_ESCAPE);
  GetWidget()->Deactivate();
  return true;
}

const char* FloatingAccessibilityBubbleView::GetClassName() const {
  return "FloatingAccessibilityBubbleView";
}

FloatingAccessibilityView::FloatingAccessibilityView(Delegate* delegate)
    : delegate_(delegate) {
  std::unique_ptr<views::BoxLayout> layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(), 0);
  SetLayoutManager(std::move(layout));

  Shelf* shelf = RootWindowController::ForTargetRootWindow()->shelf();
  std::unique_ptr<views::View> feature_buttons_container =
      CreateButtonRowContainer(kPanelPositionButtonPadding);
  dictation_button_ = feature_buttons_container->AddChildView(
      std::make_unique<DictationButtonTray>(shelf));
  select_to_speak_button_ = feature_buttons_container->AddChildView(
      std::make_unique<SelectToSpeakTray>(shelf));
  virtual_keyboard_button_ = feature_buttons_container->AddChildView(
      std::make_unique<VirtualKeyboardTray>(shelf));

  // It will be visible again as soon as any of the children becomes visible.
  feature_buttons_container->SetVisible(false);

  std::unique_ptr<views::View> tray_button_container =
      CreateButtonRowContainer(kUnifiedTopShortcutSpacing);
  a11y_tray_button_ =
      tray_button_container->AddChildView(std::make_unique<FloatingMenuButton>(
          this, kUnifiedMenuAccessibilityIcon,
          IDS_ASH_FLOATING_ACCESSIBILITY_DETAILED_MENU_OPEN,
          /*flip_for_rtl*/ true));

  std::unique_ptr<views::View> position_button_container =
      CreateButtonRowContainer(kPanelPositionButtonPadding);
  position_button_ = position_button_container->AddChildView(
      std::make_unique<FloatingMenuButton>(
          this, kAutoclickPositionBottomLeftIcon,
          IDS_ASH_AUTOCLICK_OPTION_CHANGE_POSITION, /*flip_for_rtl*/ false,
          kPanelPositionButtonSize, false, /* is_a11y_togglable */ false));

  AddChildView(std::move(feature_buttons_container));
  AddChildView(std::move(tray_button_container));
  AddChildView(CreateSeparator());
  AddChildView(std::move(position_button_container));

  // Set view IDs for testing.
  position_button_->SetId(static_cast<int>(ButtonId::kPosition));
  a11y_tray_button_->SetId(static_cast<int>(ButtonId::kSettingsList));
  dictation_button_->SetID(static_cast<int>(ButtonId::kDictation));
  select_to_speak_button_->SetID(static_cast<int>(ButtonId::kSelectToSpeak));
  virtual_keyboard_button_->SetID(static_cast<int>(ButtonId::kVirtualKeyboard));
}

FloatingAccessibilityView::~FloatingAccessibilityView() {}

void FloatingAccessibilityView::Initialize() {
  for (auto* feature_view :
       {dictation_button_, select_to_speak_button_, virtual_keyboard_button_}) {
    feature_view->Initialize();
    feature_view->CalculateTargetBounds();
    feature_view->UpdateLayout();
    feature_view->AddObserver(this);
  }
}

void FloatingAccessibilityView::SetMenuPosition(FloatingMenuPosition position) {
  switch (position) {
    case FloatingMenuPosition::kBottomRight:
      position_button_->SetVectorIcon(kAutoclickPositionBottomRightIcon);
      return;
    case FloatingMenuPosition::kBottomLeft:
      position_button_->SetVectorIcon(kAutoclickPositionBottomLeftIcon);
      return;
    case FloatingMenuPosition::kTopLeft:
      position_button_->SetVectorIcon(kAutoclickPositionTopLeftIcon);
      return;
    case FloatingMenuPosition::kTopRight:
      position_button_->SetVectorIcon(kAutoclickPositionTopRightIcon);
      return;
    case FloatingMenuPosition::kSystemDefault:
      position_button_->SetVectorIcon(base::i18n::IsRTL()
                                          ? kAutoclickPositionBottomLeftIcon
                                          : kAutoclickPositionBottomRightIcon);
      return;
  }
}

void FloatingAccessibilityView::SetDetailedViewShown(bool shown) {
  a11y_tray_button_->SetToggled(shown);
}

void FloatingAccessibilityView::FocusOnDetailedViewButton() {
  a11y_tray_button_->RequestFocus();
}

void FloatingAccessibilityView::ButtonPressed(views::Button* sender,
                                              const ui::Event& event) {
  if (sender == a11y_tray_button_) {
    delegate_->OnDetailedMenuEnabled(!a11y_tray_button_->IsToggled());
    return;
  }

  if (sender == position_button_) {
    FloatingMenuPosition new_position;
    // Rotate clockwise throughout the screen positions.
    switch (
        Shell::Get()->accessibility_controller()->GetFloatingMenuPosition()) {
      case FloatingMenuPosition::kBottomRight:
        new_position = FloatingMenuPosition::kBottomLeft;
        break;
      case FloatingMenuPosition::kBottomLeft:
        new_position = FloatingMenuPosition::kTopLeft;
        break;
      case FloatingMenuPosition::kTopLeft:
        new_position = FloatingMenuPosition::kTopRight;
        break;
      case FloatingMenuPosition::kTopRight:
        new_position = FloatingMenuPosition::kBottomRight;
        break;
      case FloatingMenuPosition::kSystemDefault:
        new_position = base::i18n::IsRTL() ? FloatingMenuPosition::kTopLeft
                                           : FloatingMenuPosition::kBottomLeft;
        break;
    }
    Shell::Get()->accessibility_controller()->SetFloatingMenuPosition(
        new_position);
    Shell::Get()
        ->accessibility_controller()
        ->TriggerAccessibilityAlertWithMessage(
            GetDescriptionForMovedToPosition(new_position));
  }
  return;
}

const char* FloatingAccessibilityView::GetClassName() const {
  return "AccessiblityFloatingView";
}

void FloatingAccessibilityView::OnViewVisibilityChanged(
    views::View* observed_view,
    views::View* starting_view) {
  if (observed_view != starting_view)
    return;
  delegate_->OnLayoutChanged();
}

}  // namespace ash
