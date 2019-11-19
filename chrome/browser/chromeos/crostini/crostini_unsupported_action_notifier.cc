// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_unsupported_action_notifier.h"

#include <utility>

#include "ash/public/cpp/app_types.h"
#include "ash/public/cpp/keyboard/keyboard_controller.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/public/cpp/toast_manager.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/accessibility/magnification_manager.h"
#include "chrome/grit/generated_resources.h"
#include "components/exo/wm_helper.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/ime/chromeos/input_method_util.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// This class must be kept in sync with CrostiniUnsupportedNotificationReason in
// enums.xml
enum class NotificationReason {
  kTabletMode = 0,
  kVirtualKeyboard = 1,
  kUnsupportedIME = 2,
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
  delegate_->AddInputMethodObserver(this);
  delegate_->AddKeyboardControllerObserver(this);
}

CrostiniUnsupportedActionNotifier::~CrostiniUnsupportedActionNotifier() {
  delegate_->RemoveTabletModeObserver(this);
  delegate_->RemoveFocusObserver(this);
  delegate_->RemoveInputMethodObserver(this);
  delegate_->RemoveKeyboardControllerObserver(this);
}

// Testing on using Debian/stretch on Eve shows Crostini supports all tested xkb
// IMEs but no non-xkb IMEs.
bool CrostiniUnsupportedActionNotifier::IsIMESupportedByCrostini(
    const chromeos::input_method::InputMethodDescriptor& method) {
  return method.id().find("xkb:") != std::string::npos;
}

void CrostiniUnsupportedActionNotifier::OnTabletModeStarted() {
  ShowVirtualKeyboardUnsupportedNotifictionIfNeeded();
}

void CrostiniUnsupportedActionNotifier::OnWindowFocused(
    aura::Window* gained_focus,
    aura::Window* lost_focus) {
  ShowVirtualKeyboardUnsupportedNotifictionIfNeeded();
  ShowIMEUnsupportedNotifictionIfNeeded();
}

void CrostiniUnsupportedActionNotifier::InputMethodChanged(
    chromeos::input_method::InputMethodManager* manager,
    Profile* profile,
    bool show_message) {
  ShowIMEUnsupportedNotifictionIfNeeded();
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
        /*text=*/
        l10n_util::GetStringUTF16(IDS_CROSTINI_UNSUPPORTED_VIRTUAL_KEYBOARD),
        /*timeout_ms=*/delegate_->ToastTimeoutMs(),
        /*dismiss_text=*/base::nullopt};
    delegate_->ShowToast(data);
    virtual_keyboard_unsupported_message_shown_ = true;
    EmitMetricReasonShown(reason);
  }
}

void CrostiniUnsupportedActionNotifier::
    ShowIMEUnsupportedNotifictionIfNeeded() {
  auto method = delegate_->GetCurrentInputMethod();
  if (IsIMESupportedByCrostini(method) ||
      !delegate_->IsFocusedWindowCrostini()) {
    return;
  }
  EmitMetricReasonTriggered(NotificationReason::kUnsupportedIME);
  if (!ime_unsupported_message_shown_) {
    auto ime_name =
        base::UTF8ToUTF16(delegate_->GetLocalizedDisplayName(method));
    ash::ToastData data = {
        /*id=*/"IMEUnsupportedInCrostini",
        /*text=*/
        l10n_util::GetStringFUTF16(IDS_CROSTINI_UNSUPPORTED_IME, ime_name),
        /*timeout_ms=*/delegate_->ToastTimeoutMs(),
        /*dismiss_text=*/base::nullopt};
    delegate_->ShowToast(data);
    ime_unsupported_message_shown_ = true;
    EmitMetricReasonShown(NotificationReason::kUnsupportedIME);
  }
}  // namespace crostini

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

chromeos::input_method::InputMethodDescriptor
CrostiniUnsupportedActionNotifier::Delegate::GetCurrentInputMethod() {
  return chromeos::input_method::InputMethodManager::Get()
      ->GetActiveIMEState()
      ->GetCurrentInputMethod();
}

bool CrostiniUnsupportedActionNotifier::Delegate::IsVirtualKeyboardVisible() {
  return ash::KeyboardController::Get()->IsKeyboardVisible();
}

void CrostiniUnsupportedActionNotifier::Delegate::ShowToast(
    const ash::ToastData& toast_data) {
  ash::ToastManager::Get()->Show(toast_data);
}

std::string
CrostiniUnsupportedActionNotifier::Delegate::GetLocalizedDisplayName(
    const chromeos::input_method::InputMethodDescriptor& descriptor) {
  return chromeos::input_method::InputMethodManager::Get()
      ->GetInputMethodUtil()
      ->GetLocalizedDisplayName(descriptor);
}

int CrostiniUnsupportedActionNotifier::Delegate::ToastTimeoutMs() {
  auto* manager = chromeos::MagnificationManager::Get();
  if (manager &&
      (manager->IsMagnifierEnabled() || manager->IsDockedMagnifierEnabled())) {
    return 60 * 1000;
  } else {
    return 5 * 1000;
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

void CrostiniUnsupportedActionNotifier::Delegate::AddInputMethodObserver(
    chromeos::input_method::InputMethodManager::Observer* observer) {
  chromeos::input_method::InputMethodManager::Get()->AddObserver(observer);
}

void CrostiniUnsupportedActionNotifier::Delegate::RemoveInputMethodObserver(
    chromeos::input_method::InputMethodManager::Observer* observer) {
  chromeos::input_method::InputMethodManager::Get()->RemoveObserver(observer);
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
