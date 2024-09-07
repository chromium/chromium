// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/floating_accessibility_view.h"

#include "ash/accessibility/accessibility_controller.h"
#include "ash/ime/ime_controller_impl.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/keyboard/keyboard_controller.h"
#include "ash/public/cpp/system_tray.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/accessibility/dictation_button_tray.h"
#include "ash/system/accessibility/floating_menu_button.h"
#include "ash/system/accessibility/select_to_speak/select_to_speak_tray.h"
#include "ash/system/ime_menu/ime_menu_tray.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/virtual_keyboard/virtual_keyboard_tray.h"
#include "base/functional/bind.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

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
    for (views::View* view : children()) {
      any_visible |= view->GetVisible();
    }
    SetVisible(any_visible);
  }
};

std::unique_ptr<views::Separator> CreateSeparator() {
  auto separator = std::make_unique<views::Separator>();
  separator->SetColorId(ui::kColorAshSystemUIMenuSeparator);
  separator->SetPreferredLength(kSeparatorHeight);
  int total_height = kUnifiedTopShortcutSpacing * 2 + kTrayItemSize;
  int separator_spacing = (total_height - kSeparatorHeight) / 2;
  separator->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::TLBR(separator_spacing - kUnifiedTopShortcutSpacing, 0,
                        separator_spacing, 0)));
  return separator;
}

std::unique_ptr<views::View> CreateButtonRowContainer(int padding) {
  auto button_container = std::make_unique<DynamicRowView>();
  button_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets::TLBR(0, padding, padding, padding), padding));
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
  }
}

bool IsKioskImeButtonEnabled() {
  return Shell::Get()->session_controller()->IsRunningInAppMode() &&
         base::FeatureList::IsEnabled(features::kKioskEnableImeButton) &&
         Shell::Get()->ime_controller()->GetVisibleImes().size() > 1;
}

}  // namespace

FloatingAccessibilityBubbleView::FloatingAccessibilityBubbleView(
    const TrayBubbleView::InitParams& init_params)
    : TrayBubbleView(init_params) {
  // Intercept ESC keypresses.
  AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));

  GetViewAccessibility().SetRole(ax::mojom::Role::kWindow);
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

void FloatingAccessibilityBubbleView::GetAccessibleNodeData(
    ui::AXNodeData* node_data) {
  node_data->SetNameExplicitlyEmpty();
  TrayBubbleView::GetAccessibleNodeData(node_data);
}

BEGIN_METADATA(FloatingAccessibilityBubbleView)
END_METADATA

FloatingAccessibilityView::FloatingAccessibilityView(Delegate* delegate)
    : delegate_(delegate) {
  Shelf* shelf = RootWindowController::ForTargetRootWindow()->shelf();
  std::unique_ptr<views::View> feature_buttons_container =
      CreateButtonRowContainer(kPanelPositionButtonPadding);
  dictation_button_observation_.Observe(feature_buttons_container->AddChildView(
      std::make_unique<DictationButtonTray>(
          shelf, TrayBackgroundViewCatalogName::kDictationAccesibilityWindow)));
  select_to_speak_button_observation_.Observe(
      feature_buttons_container->AddChildView(std::make_unique<
                                              SelectToSpeakTray>(
          shelf,
          TrayBackgroundViewCatalogName::kSelectToSpeakAccessibilityWindow)));
  virtual_keyboard_button_observation_.Observe(
      feature_buttons_container->AddChildView(std::make_unique<
                                              VirtualKeyboardTray>(
          shelf,
          TrayBackgroundViewCatalogName::kVirtualKeyboardAccessibilityWindow)));

  // It will be visible again as soon as any of the children becomes visible.
  feature_buttons_container->SetVisible(false);

  std::unique_ptr<views::View> tray_button_container =
      CreateButtonRowContainer(kUnifiedTopShortcutSpacing);
  a11y_tray_button_ =
      tray_button_container->AddChildView(std::make_unique<FloatingMenuButton>(
          base::BindRepeating(
              &FloatingAccessibilityView::OnA11yTrayButtonPressed,
              base::Unretained(this)),
          kUnifiedMenuAccessibilityIcon,
          IDS_ASH_FLOATING_ACCESSIBILITY_DETAILED_MENU_OPEN,
          /*flip_for_rtl*/ true));

  std::unique_ptr<views::View> position_button_container =
      CreateButtonRowContainer(kPanelPositionButtonPadding);
  position_button_ = position_button_container->AddChildView(
      std::make_unique<FloatingMenuButton>(
          base::BindRepeating(
              &FloatingAccessibilityView::OnPositionButtonPressed,
              base::Unretained(this)),
          kAutoclickPositionBottomLeftIcon,
          IDS_ASH_AUTOCLICK_OPTION_CHANGE_POSITION, /*flip_for_rtl*/ false,
          kPanelPositionButtonSize, false, /* is_a11y_togglable */ false));

  if (IsKioskImeButtonEnabled()) {
    Shell::Get()->ime_controller()->SetExtraInputOptionsEnabledState(
        /*is_extra_input_options_enabled*/ false, /*is_emoji_enabled*/ false,
        /*is_handwriting_enabled*/ false, /*is_voice_enabled*/ false);
    std::unique_ptr<views::View> ime_button_container =
        CreateButtonRowContainer(kPanelPositionButtonPadding);
    ime_button_observation_.Observe(ime_button_container->AddChildView(
        std::make_unique<ImeMenuTray>(shelf)));
    ime_button_container->SetVisible(true);
    ime_button()->SetVisiblePreferred(true);

    AddChildView(std::move(ime_button_container));
    AddChildView(CreateSeparator());
  }

  AddChildView(std::move(feature_buttons_container));
  AddChildView(std::move(tray_button_container));
  AddChildView(CreateSeparator());
  AddChildView(std::move(position_button_container));

  // Set view IDs for testing.
  position_button_->SetID(static_cast<int>(ButtonId::kPosition));
  a11y_tray_button_->SetID(static_cast<int>(ButtonId::kSettingsList));
  dictation_button()->SetID(static_cast<int>(ButtonId::kDictation));
  select_to_speak_button()->SetID(static_cast<int>(ButtonId::kSelectToSpeak));
  virtual_keyboard_button()->SetID(
      static_cast<int>(ButtonId::kVirtualKeyboard));
  if (IsKioskImeButtonEnabled()) {
    ime_button()->SetID(static_cast<int>(ButtonId::kIme));
  }
}

FloatingAccessibilityView::~FloatingAccessibilityView() {
  KeyboardController::Get()->RemoveObserver(this);
  Shell::Get()->system_tray_notifier()->RemoveSystemTrayObserver(this);
}

void FloatingAccessibilityView::Initialize() {
  Shell::Get()->system_tray_notifier()->AddSystemTrayObserver(this);
  KeyboardController::Get()->AddObserver(this);
  for (TrayBackgroundView* feature_view : {
           dictation_button(),
           select_to_speak_button(),
           virtual_keyboard_button(),
       }) {
    feature_view->Initialize();
    feature_view->CalculateTargetBounds();
    feature_view->UpdateLayout();
  }
  if (IsKioskImeButtonEnabled()) {
    ime_button()->Initialize();
    ime_button()->CalculateTargetBounds();
    ime_button()->UpdateLayout();
    ime_button()->SetVisible(true);
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

void FloatingAccessibilityView::OnA11yTrayButtonPressed() {
  delegate_->OnDetailedMenuEnabled(!a11y_tray_button_->GetToggled());
}

void FloatingAccessibilityView::OnPositionButtonPressed() {
  FloatingMenuPosition new_position;
  // Rotate clockwise throughout the screen positions.
  switch (Shell::Get()->accessibility_controller()->GetFloatingMenuPosition()) {
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

void FloatingAccessibilityView::OnViewVisibilityChanged(
    views::View* observed_view,
    views::View* starting_view) {
  if (observed_view != starting_view)
    return;
  delegate_->OnLayoutChanged();
}

void FloatingAccessibilityView::OnFocusLeavingSystemTray(bool reverse) {}

void FloatingAccessibilityView::OnImeMenuTrayBubbleShown() {
  delegate_->OnDetailedMenuEnabled(false);
}

void FloatingAccessibilityView::OnKeyboardVisibilityChanged(bool visible) {
  // To avoid the collision with the virtual keyboard
  // Accessibility tray is closed after opening the virtual keyboard tray
  if (visible)
    delegate_->OnDetailedMenuEnabled(false);
}

BEGIN_METADATA(FloatingAccessibilityView)
END_METADATA

}  // namespace ash
