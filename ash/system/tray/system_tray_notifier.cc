// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/system_tray_notifier.h"

#include "ash/system/ime/ime_observer.h"
#include "ash/system/network/network_observer.h"
#include "ash/system/privacy/screen_security_observer.h"
#include "ash/system/tray/system_tray_observer.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "ash/system/virtual_keyboard/virtual_keyboard_observer.h"

namespace ash {

SystemTrayNotifier::SystemTrayNotifier() = default;

SystemTrayNotifier::~SystemTrayNotifier() = default;

void SystemTrayNotifier::AddIMEObserver(IMEObserver* observer) {
  ime_observers_.AddObserver(observer);
}

void SystemTrayNotifier::RemoveIMEObserver(IMEObserver* observer) {
  ime_observers_.RemoveObserver(observer);
}

void SystemTrayNotifier::NotifyRefreshIME() {
  for (auto& observer : ime_observers_)
    observer.OnIMERefresh();
}

void SystemTrayNotifier::NotifyRefreshIMEMenu(bool is_active) {
  for (auto& observer : ime_observers_)
    observer.OnIMEMenuActivationChanged(is_active);
}

void SystemTrayNotifier::AddNetworkObserver(NetworkObserver* observer) {
  network_observers_.AddObserver(observer);
}

void SystemTrayNotifier::RemoveNetworkObserver(NetworkObserver* observer) {
  network_observers_.RemoveObserver(observer);
}

void SystemTrayNotifier::NotifyRequestToggleWifi() {
  for (auto& observer : network_observers_)
    observer.RequestToggleWifi();
}

void SystemTrayNotifier::AddScreenSecurityObserver(
    ScreenSecurityObserver* observer) {
  screen_security_observers_.AddObserver(observer);
}

void SystemTrayNotifier::RemoveScreenSecurityObserver(
    ScreenSecurityObserver* observer) {
  screen_security_observers_.RemoveObserver(observer);
}

void SystemTrayNotifier::NotifyScreenAccessStart(
    base::RepeatingClosure stop_callback,
    base::RepeatingClosure source_callback,
    const std::u16string& access_app_name) {
  for (auto& observer : screen_security_observers_) {
    observer.OnScreenAccessStart(stop_callback, source_callback,
                                 access_app_name);
  }
}

void SystemTrayNotifier::NotifyScreenAccessStop() {
  for (auto& observer : screen_security_observers_) {
    observer.OnScreenAccessStop();
  }
}

void SystemTrayNotifier::NotifyRemotingScreenShareStart(
    base::RepeatingClosure stop_callback) {
  for (auto& observer : screen_security_observers_) {
    observer.OnRemotingScreenShareStart(stop_callback);
  }
}

void SystemTrayNotifier::NotifyRemotingScreenShareStop() {
  for (auto& observer : screen_security_observers_) {
    observer.OnRemotingScreenShareStop();
  }
}

void SystemTrayNotifier::AddSystemTrayObserver(SystemTrayObserver* observer) {
  system_tray_observers_.AddObserver(observer);
}

void SystemTrayNotifier::RemoveSystemTrayObserver(
    SystemTrayObserver* observer) {
  system_tray_observers_.RemoveObserver(observer);
}

void SystemTrayNotifier::NotifyFocusOut(bool reverse) {
  for (auto& observer : system_tray_observers_)
    observer.OnFocusLeavingSystemTray(reverse);
}

void SystemTrayNotifier::NotifySystemTrayBubbleShown() {
  for (auto& observer : system_tray_observers_)
    observer.OnSystemTrayBubbleShown();
}

void SystemTrayNotifier::NotifyStatusAreaAnchoredBubbleVisibilityChanged(
    TrayBubbleView* tray_bubble,
    bool visible) {
  for (auto& observer : system_tray_observers_) {
    observer.OnStatusAreaAnchoredBubbleVisibilityChanged(tray_bubble, visible);
  }
}

void SystemTrayNotifier::NotifyTrayBubbleBoundsChanged(
    TrayBubbleView* tray_bubble) {
  for (auto& observer : system_tray_observers_) {
    observer.OnTrayBubbleBoundsChanged(tray_bubble);
  }
}

void SystemTrayNotifier::NotifyImeMenuTrayBubbleShown() {
  for (auto& observer : system_tray_observers_) {
    observer.OnImeMenuTrayBubbleShown();
  }
}

void SystemTrayNotifier::AddVirtualKeyboardObserver(
    VirtualKeyboardObserver* observer) {
  virtual_keyboard_observers_.AddObserver(observer);
}

void SystemTrayNotifier::RemoveVirtualKeyboardObserver(
    VirtualKeyboardObserver* observer) {
  virtual_keyboard_observers_.RemoveObserver(observer);
}

void SystemTrayNotifier::NotifyVirtualKeyboardSuppressionChanged(
    bool suppressed) {
  for (auto& observer : virtual_keyboard_observers_)
    observer.OnKeyboardSuppressionChanged(suppressed);
}

}  // namespace ash
