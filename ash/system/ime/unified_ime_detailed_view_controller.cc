// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/ime/unified_ime_detailed_view_controller.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/ime/ime_controller.h"
#include "ash/shell.h"
#include "ash/system/ime/tray_ime_chromeos.h"
#include "ash/system/tray/detailed_view_delegate.h"
#include "ash/system/tray/system_tray_notifier.h"

namespace ash {

namespace {

ImeListView::SingleImeBehavior GetSingleImeBehavior() {
  return Shell::Get()->ime_controller()->managed_by_policy()
             ? ImeListView::SHOW_SINGLE_IME
             : ImeListView::HIDE_SINGLE_IME;
}

}  // namespace

UnifiedIMEDetailedViewController::UnifiedIMEDetailedViewController(
    UnifiedSystemTrayController* tray_controller)
    : detailed_view_delegate_(
          std::make_unique<DetailedViewDelegate>(tray_controller)) {
  Shell::Get()->system_tray_notifier()->AddIMEObserver(this);
  Shell::Get()->system_tray_notifier()->AddVirtualKeyboardObserver(this);
  Shell::Get()->accessibility_controller()->AddObserver(this);
}

UnifiedIMEDetailedViewController::~UnifiedIMEDetailedViewController() {
  Shell::Get()->system_tray_notifier()->RemoveIMEObserver(this);
  Shell::Get()->system_tray_notifier()->RemoveVirtualKeyboardObserver(this);
  Shell::Get()->accessibility_controller()->RemoveObserver(this);
}

views::View* UnifiedIMEDetailedViewController::CreateView() {
  DCHECK(!view_);
  view_ = new tray::IMEDetailedView(detailed_view_delegate_.get(),
                                    Shell::Get()->ime_controller());
  view_->Init(ShouldShowKeyboardToggle(), GetSingleImeBehavior());
  return view_;
}

void UnifiedIMEDetailedViewController::OnKeyboardSuppressionChanged(
    bool suppressed) {
  keyboard_suppressed_ = suppressed;
  Update();
}

void UnifiedIMEDetailedViewController::OnAccessibilityStatusChanged() {
  Update();
}

void UnifiedIMEDetailedViewController::OnIMERefresh() {
  Update();
}

void UnifiedIMEDetailedViewController::OnIMEMenuActivationChanged(
    bool is_active) {
  Update();
}

void UnifiedIMEDetailedViewController::Update() {
  ImeController* ime_controller = Shell::Get()->ime_controller();
  view_->Update(ime_controller->current_ime().id,
                ime_controller->available_imes(),
                ime_controller->current_ime_menu_items(),
                ShouldShowKeyboardToggle(), GetSingleImeBehavior());
}

bool UnifiedIMEDetailedViewController::ShouldShowKeyboardToggle() const {
  return keyboard_suppressed_ &&
         !Shell::Get()->accessibility_controller()->virtual_keyboard_enabled();
}

}  // namespace ash
