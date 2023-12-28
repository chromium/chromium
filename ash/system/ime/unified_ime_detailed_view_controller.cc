// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/ime/unified_ime_detailed_view_controller.h"

#include "ash/accessibility/accessibility_controller.h"
#include "ash/ime/ime_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/ime/ime_detailed_view.h"
#include "ash/system/tray/detailed_view_delegate.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ui/base/l10n/l10n_util.h"

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

std::unique_ptr<views::View> UnifiedIMEDetailedViewController::CreateView() {
  DCHECK(!view_);
  auto view = std::make_unique<IMEDetailedView>(detailed_view_delegate_.get(),
                                                Shell::Get()->ime_controller());
  view_ = view.get();
  view_->Init(ShouldShowKeyboardToggle(), GetSingleImeBehavior());
  return view;
}

std::u16string UnifiedIMEDetailedViewController::GetAccessibleName() const {
  return l10n_util::GetStringUTF16(
      IDS_ASH_QUICK_SETTINGS_BUBBLE_IME_SETTINGS_ACCESSIBLE_DESCRIPTION);
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
  ImeControllerImpl* ime_controller = Shell::Get()->ime_controller();
  view_->Update(ime_controller->current_ime().id,
                ime_controller->GetVisibleImes(),
                ime_controller->current_ime_menu_items(),
                ShouldShowKeyboardToggle(), GetSingleImeBehavior());
}

bool UnifiedIMEDetailedViewController::ShouldShowKeyboardToggle() const {
  return keyboard_suppressed_ && !Shell::Get()
                                      ->accessibility_controller()
                                      ->virtual_keyboard()
                                      .enabled();
}

}  // namespace ash
