// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/crostini_unsupported_action_notifier.h"

#include <utility>

#include "ash/constants/app_types.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/keyboard/keyboard_controller.h"
#include "ash/public/cpp/system/toast_manager.h"
#include "ash/public/cpp/tablet_mode.h"
#include "base/check.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/accessibility/magnification_manager.h"
#include "chrome/grit/generated_resources.h"
#include "components/exo/wm_helper.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/ime/ash/input_method_util.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// This class must be kept in sync with CrostiniUnsupportedNotificationReason in
// enums.xml
enum class NotificationReason {
  kTabletMode = 0,
  kVirtualKeyboard = 1,
  kUnsupportedIME = 2,  // Removed in M120.
  kMaxValue = kUnsupportedIME,
};

void EmitMetricReasonTriggered(NotificationReason reason) {
  UMA_HISTOGRAM_ENUMERATION("Crostini.UnsupportedNotification.Reason.Triggered",
                            reason);
}
void EmitMetricReasonShown(NotificationReason reason) {
  UMA_HISTOGRAM_ENUMERATION("Crostini.UnsupportedNotification.Reason.Shown",
                            reason);
}
}  // namespace

namespace crostini {

CrostiniUnsupportedActionNotifier::CrostiniUnsupportedActionNotifier()
    : CrostiniUnsupportedActionNotifier(std::make_unique<Delegate>()) {}

CrostiniUnsupportedActionNotifier::CrostiniUnsupportedActionNotifier(
    std::unique_ptr<Delegate> delegate)
    : delegate_(std::move(delegate)) {
  delegate_->AddTabletModeObserver(this);
  delegate_->AddFocusObserver(this);
  delegate_->AddKeyboardControllerObserver(this);
}

CrostiniUnsupportedActionNotifier::~CrostiniUnsupportedActionNotifier() {
  delegate_->RemoveTabletModeObserver(this);
  delegate_->RemoveFocusObserver(this);
  delegate_->RemoveKeyboardControllerObserver(this);
}

void CrostiniUnsupportedActionNotifier::OnTabletModeStarted() {
  ShowVirtualKeyboardUnsupportedNotifictionIfNeeded();
}

void CrostiniUnsupportedActionNotifier::OnWindowFocused(
    aura::Window* gained_focus,
    aura::Window* lost_focus) {
  ShowVirtualKeyboardUnsupportedNotifictionIfNeeded();
}

void CrostiniUnsupportedActionNotifier::OnKeyboardVisibilityChanged(
    bool visible) {
  if (visible) {
    ShowVirtualKeyboardUnsupportedNotifictionIfNeeded();
  }
}

void CrostiniUnsupportedActionNotifier::
    ShowVirtualKeyboardUnsupportedNotifictionIfNeeded() {
  if (!delegate_->IsFocusedWindowCrostini()) {
    return;
  }
  NotificationReason reason;
  if (delegate_->IsVirtualKeyboardVisible()) {
    reason = NotificationReason::kVirtualKeyboard;
  } else if (delegate_->IsInTabletMode()) {
    reason = NotificationReason::kTabletMode;
  } else {
    return;
  }
  EmitMetricReasonTriggered(reason);
  if (!virtual_keyboard_unsupported_message_shown_) {
    ash::ToastData data = {
        /*id=*/"VKUnsupportedInCrostini",
        ash::ToastCatalogName::kCrostiniUnsupportedVirtualKeyboard,
        /*text=*/
        l10n_util::GetStringUTF16(IDS_CROSTINI_UNSUPPORTED_VIRTUAL_KEYBOARD),
        delegate_->ToastTimeout()};
    delegate_->ShowToast(std::move(data));
    virtual_keyboard_unsupported_message_shown_ = true;
    EmitMetricReasonShown(reason);
  }
}

CrostiniUnsupportedActionNotifier::Delegate::Delegate() = default;

CrostiniUnsupportedActionNotifier::Delegate::~Delegate() = default;

bool CrostiniUnsupportedActionNotifier::Delegate::IsInTabletMode() {
  return ash::TabletMode::Get()->InTabletMode();
}

bool CrostiniUnsupportedActionNotifier::Delegate::IsFocusedWindowCrostini() {
  if (!exo::WMHelper::HasInstance()) {
    return false;
  }
  auto* focused_window = exo::WMHelper::GetInstance()->GetFocusedWindow();
  return focused_window &&
         (focused_window->GetProperty(aura::client::kAppType) ==
          static_cast<int>(ash::AppType::CROSTINI_APP));
}

bool CrostiniUnsupportedActionNotifier::Delegate::IsVirtualKeyboardVisible() {
  return ash::KeyboardController::Get()->IsKeyboardVisible();
}

void CrostiniUnsupportedActionNotifier::Delegate::ShowToast(
    ash::ToastData toast_data) {
  ash::ToastManager::Get()->Show(std::move(toast_data));
}

base::TimeDelta CrostiniUnsupportedActionNotifier::Delegate::ToastTimeout() {
  auto* manager = ash::MagnificationManager::Get();
  if (manager &&
      (manager->IsMagnifierEnabled() || manager->IsDockedMagnifierEnabled())) {
    return base::Seconds(60);
  } else {
    return ash::ToastData::kDefaultToastDuration;
  }
}

void CrostiniUnsupportedActionNotifier::Delegate::AddFocusObserver(
    aura::client::FocusChangeObserver* observer) {
  if (exo::WMHelper::HasInstance()) {
    exo::WMHelper::GetInstance()->AddFocusObserver(observer);
  }
}

void CrostiniUnsupportedActionNotifier::Delegate::RemoveFocusObserver(
    aura::client::FocusChangeObserver* observer) {
  if (exo::WMHelper::HasInstance()) {
    exo::WMHelper::GetInstance()->RemoveFocusObserver(observer);
  }
}

void CrostiniUnsupportedActionNotifier::Delegate::AddTabletModeObserver(
    TabletModeObserver* observer) {
  auto* client = ash::TabletMode::Get();
  DCHECK(client);
  client->AddObserver(observer);
}

void CrostiniUnsupportedActionNotifier::Delegate::RemoveTabletModeObserver(
    TabletModeObserver* observer) {
  auto* client = ash::TabletMode::Get();
  DCHECK(client);
  client->RemoveObserver(observer);
}

void CrostiniUnsupportedActionNotifier::Delegate::AddKeyboardControllerObserver(
    ash::KeyboardControllerObserver* observer) {
  ash::KeyboardController::Get()->AddObserver(observer);
}
void CrostiniUnsupportedActionNotifier::Delegate::
    RemoveKeyboardControllerObserver(
        ash::KeyboardControllerObserver* observer) {
  ash::KeyboardController::Get()->RemoveObserver(observer);
}

}  // namespace crostini
