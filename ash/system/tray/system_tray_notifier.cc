// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/system_tray_notifier.h"

#include "ash/public/cpp/system_tray_observer.h"
#include "ash/system/ime/ime_observer.h"
#include "ash/system/network/network_observer.h"
#include "ash/system/privacy/screen_capture_observer.h"
#include "ash/system/privacy/screen_share_observer.h"
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

void SystemTrayNotifier::AddScreenCaptureObserver(
    ScreenCaptureObserver* observer) {
  screen_capture_observers_.AddObserver(observer);
}

void SystemTrayNotifier::RemoveScreenCaptureObserver(
    ScreenCaptureObserver* observer) {
  screen_capture_observers_.RemoveObserver(observer);
}

void SystemTrayNotifier::NotifyScreenCaptureStart(
    base::RepeatingClosure stop_callback,
    base::RepeatingClosure source_callback,
    const std::u16string& sharing_app_name) {
  for (auto& observer : screen_capture_observers_)
    observer.OnScreenCaptureStart(stop_callback, source_callback,
                                  sharing_app_name);
}

void SystemTrayNotifier::NotifyScreenCaptureStop() {
  for (auto& observer : screen_capture_observers_)
    observer.OnScreenCaptureStop();
}

void SystemTrayNotifier::AddScreenShareObserver(ScreenShareObserver* observer) {
  screen_share_observers_.AddObserver(observer);
}

void SystemTrayNotifier::RemoveScreenShareObserver(
    ScreenShareObserver* observer) {
  screen_share_observers_.RemoveObserver(observer);
}

void SystemTrayNotifier::NotifyScreenShareStart(
    base::RepeatingClosure stop_callback,
    const std::u16string& helper_name) {
  for (auto& observer : screen_share_observers_)
    observer.OnScreenShareStart(stop_callback, helper_name);
}

void SystemTrayNotifier::NotifyScreenShareStop() {
  for (auto& observer : screen_share_observers_)
    observer.OnScreenShareStop();
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
