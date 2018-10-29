// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/status_area_widget.h"

#include "ash/session/session_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/accessibility/dictation_button_tray.h"
#include "ash/system/accessibility/select_to_speak_tray.h"
#include "ash/system/flag_warning/flag_warning_tray.h"
#include "ash/system/ime_menu/ime_menu_tray.h"
#include "ash/system/message_center/notification_tray.h"
#include "ash/system/overview/overview_button_tray.h"
#include "ash/system/palette/palette_tray.h"
#include "ash/system/session/logout_button_tray.h"
#include "ash/system/status_area_widget_delegate.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/virtual_keyboard/virtual_keyboard_tray.h"
#include "base/command_line.h"
#include "base/i18n/time_formatting.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/display.h"
#include "ui/native_theme/native_theme_dark_aura.h"

namespace ash {

StatusAreaWidget::StatusAreaWidget(aura::Window* status_container, Shelf* shelf)
    : status_area_widget_delegate_(new StatusAreaWidgetDelegate(shelf)),
      shelf_(shelf) {
  DCHECK(status_container);
  DCHECK(shelf);
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.delegate = status_area_widget_delegate_;
  params.name = "StatusAreaWidget";
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.parent = status_container;
  Init(params);
  set_focus_on_creation(false);
  SetContentsView(status_area_widget_delegate_);
}

void StatusAreaWidget::Initialize() {
  // Create the child views, right to left.
  overview_button_tray_ = std::make_unique<OverviewButtonTray>(shelf_);
  status_area_widget_delegate_->AddChildView(overview_button_tray_.get());

  unified_system_tray_ = std::make_unique<UnifiedSystemTray>(shelf_);
  status_area_widget_delegate_->AddChildView(unified_system_tray_.get());

  palette_tray_ = std::make_unique<PaletteTray>(shelf_);
  status_area_widget_delegate_->AddChildView(palette_tray_.get());

  virtual_keyboard_tray_ = std::make_unique<VirtualKeyboardTray>(shelf_);
  status_area_widget_delegate_->AddChildView(virtual_keyboard_tray_.get());

  ime_menu_tray_ = std::make_unique<ImeMenuTray>(shelf_);
  status_area_widget_delegate_->AddChildView(ime_menu_tray_.get());

  select_to_speak_tray_ = std::make_unique<SelectToSpeakTray>(shelf_);
  status_area_widget_delegate_->AddChildView(select_to_speak_tray_.get());

  dictation_button_tray_ = std::make_unique<DictationButtonTray>(shelf_);
  status_area_widget_delegate_->AddChildView(dictation_button_tray_.get());

  logout_button_tray_ = std::make_unique<LogoutButtonTray>(shelf_);
  status_area_widget_delegate_->AddChildView(logout_button_tray_.get());

  if (::features::IsSingleProcessMash() || ::features::IsMultiProcessMash()) {
    flag_warning_tray_ = std::make_unique<FlagWarningTray>(shelf_);
    status_area_widget_delegate_->AddChildView(flag_warning_tray_.get());
  }

  // The layout depends on the number of children, so build it once after
  // adding all of them.
  status_area_widget_delegate_->UpdateLayout();

  // Initialize after all trays have been created.
  unified_system_tray_->Initialize();
  palette_tray_->Initialize();
  virtual_keyboard_tray_->Initialize();
  ime_menu_tray_->Initialize();
  select_to_speak_tray_->Initialize();
  if (dictation_button_tray_)
    dictation_button_tray_->Initialize();
  overview_button_tray_->Initialize();
  UpdateAfterShelfAlignmentChange();
  UpdateAfterLoginStatusChange(
      Shell::Get()->session_controller()->login_status());

  // NOTE: Container may be hidden depending on login/display state.
  Show();
}

StatusAreaWidget::~StatusAreaWidget() {
  unified_system_tray_.reset();
  ime_menu_tray_.reset();
  select_to_speak_tray_.reset();
  dictation_button_tray_.reset();
  virtual_keyboard_tray_.reset();
  palette_tray_.reset();
  logout_button_tray_.reset();
  overview_button_tray_.reset();
  flag_warning_tray_.reset();

  // All child tray views have been removed.
  DCHECK_EQ(0, GetContentsView()->child_count());
}

void StatusAreaWidget::UpdateAfterShelfAlignmentChange() {
  unified_system_tray_->UpdateAfterShelfAlignmentChange();
  logout_button_tray_->UpdateAfterShelfAlignmentChange();
  virtual_keyboard_tray_->UpdateAfterShelfAlignmentChange();
  ime_menu_tray_->UpdateAfterShelfAlignmentChange();
  select_to_speak_tray_->UpdateAfterShelfAlignmentChange();
  if (dictation_button_tray_)
    dictation_button_tray_->UpdateAfterShelfAlignmentChange();
  palette_tray_->UpdateAfterShelfAlignmentChange();
  overview_button_tray_->UpdateAfterShelfAlignmentChange();
  if (flag_warning_tray_)
    flag_warning_tray_->UpdateAfterShelfAlignmentChange();
  status_area_widget_delegate_->UpdateLayout();
}

void StatusAreaWidget::UpdateAfterLoginStatusChange(LoginStatus login_status) {
  if (login_status_ == login_status)
    return;
  login_status_ = login_status;

  unified_system_tray_->UpdateAfterLoginStatusChange();
  logout_button_tray_->UpdateAfterLoginStatusChange();
  overview_button_tray_->UpdateAfterLoginStatusChange(login_status);
}

void StatusAreaWidget::SetSystemTrayVisibility(bool visible) {
  TrayBackgroundView* tray = unified_system_tray_.get();
  tray->SetVisible(visible);
  // Opacity is set to prevent flakiness in kiosk browser tests. See
  // https://crbug.com/624584.
  SetOpacity(visible ? 1.f : 0.f);
  if (visible) {
    Show();
  } else {
    tray->CloseBubble();
    Hide();
  }
}

TrayBackgroundView* StatusAreaWidget::GetSystemTrayAnchor() const {
  // Use the target visibility of the layer instead of the visibility of the
  // view because the view is still visible when fading away, but we do not want
  // to anchor to this element in that case.
  if (overview_button_tray_->layer()->GetTargetVisibility())
    return overview_button_tray_.get();

  return unified_system_tray_.get();
}

bool StatusAreaWidget::ShouldShowShelf() const {
  // If it has main bubble, return true.
  if (unified_system_tray_->IsBubbleShown())
    return true;

  // If it has a slider bubble, return false.
  if (unified_system_tray_->IsSliderBubbleShown())
    return false;

  // All other tray bubbles will force the shelf to be visible.
  return TrayBubbleView::IsATrayBubbleOpen();
}

bool StatusAreaWidget::IsMessageBubbleShown() const {
  return unified_system_tray_->IsBubbleShown();
}

void StatusAreaWidget::SchedulePaint() {
  status_area_widget_delegate_->SchedulePaint();
  unified_system_tray_->SchedulePaint();
  virtual_keyboard_tray_->SchedulePaint();
  logout_button_tray_->SchedulePaint();
  ime_menu_tray_->SchedulePaint();
  select_to_speak_tray_->SchedulePaint();
  if (dictation_button_tray_)
    dictation_button_tray_->SchedulePaint();
  palette_tray_->SchedulePaint();
  overview_button_tray_->SchedulePaint();
  if (flag_warning_tray_)
    flag_warning_tray_->SchedulePaint();
}

const ui::NativeTheme* StatusAreaWidget::GetNativeTheme() const {
  return ui::NativeThemeDarkAura::instance();
}

bool StatusAreaWidget::OnNativeWidgetActivationChanged(bool active) {
  if (!Widget::OnNativeWidgetActivationChanged(active))
    return false;
  if (active)
    status_area_widget_delegate_->SetPaneFocusAndFocusDefault();
  return true;
}

void StatusAreaWidget::UpdateShelfItemBackground(SkColor color) {
  unified_system_tray_->UpdateShelfItemBackground(color);
  virtual_keyboard_tray_->UpdateShelfItemBackground(color);
  ime_menu_tray_->UpdateShelfItemBackground(color);
  select_to_speak_tray_->UpdateShelfItemBackground(color);
  if (dictation_button_tray_)
    dictation_button_tray_->UpdateShelfItemBackground(color);
  palette_tray_->UpdateShelfItemBackground(color);
  overview_button_tray_->UpdateShelfItemBackground(color);
}

void StatusAreaWidget::OnMouseEvent(ui::MouseEvent* event) {
  // Clicking anywhere except the virtual keyboard tray icon should hide the
  // virtual keyboard.
  gfx::Point location = event->location();
  views::View::ConvertPointFromWidget(virtual_keyboard_tray_.get(), &location);
  if (event->type() == ui::ET_MOUSE_PRESSED &&
      !virtual_keyboard_tray_->HitTestPoint(location)) {
    keyboard::KeyboardController::Get()->HideKeyboardImplicitlyByUser();
  }
  views::Widget::OnMouseEvent(event);
}

void StatusAreaWidget::OnGestureEvent(ui::GestureEvent* event) {
  // Tapping anywhere except the virtual keyboard tray icon should hide the
  // virtual keyboard.
  gfx::Point location = event->location();
  views::View::ConvertPointFromWidget(virtual_keyboard_tray_.get(), &location);
  if (event->type() == ui::ET_GESTURE_TAP_DOWN &&
      !virtual_keyboard_tray_->HitTestPoint(location)) {
    keyboard::KeyboardController::Get()->HideKeyboardImplicitlyByUser();
  }
  views::Widget::OnGestureEvent(event);
}

}  // namespace ash
