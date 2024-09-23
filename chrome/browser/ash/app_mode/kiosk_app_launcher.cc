// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/kiosk_app_launcher.h"

#include <optional>
#include <string>

#include "chrome/browser/ash/app_mode/kiosk_app_launch_error.h"

namespace ash {

KioskAppLauncher::KioskAppLauncher() = default;

KioskAppLauncher::KioskAppLauncher(KioskAppLauncher::NetworkDelegate* delegate)
    : delegate_(delegate) {}

KioskAppLauncher::~KioskAppLauncher() = default;

KioskAppLauncher::ObserverList::ObserverList() = default;
KioskAppLauncher::ObserverList::~ObserverList() = default;
void KioskAppLauncher::ObserverList::AddObserver(
    KioskAppLauncher::Observer* observer) {
  observers_.AddObserver(observer);
}

void KioskAppLauncher::ObserverList::RemoveObserver(
    KioskAppLauncher::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void KioskAppLauncher::ObserverList::NotifyAppDataUpdated() {
  for (auto& observer : observers_) {
    observer.OnAppDataUpdated();
  }
}

void KioskAppLauncher::ObserverList::NotifyAppInstalling() {
  for (auto& observer : observers_) {
    observer.OnAppInstalling();
  }
}

void KioskAppLauncher::ObserverList::NotifyAppPrepared() {
  for (auto& observer : observers_) {
    observer.OnAppPrepared();
  }
}

void KioskAppLauncher::ObserverList::NotifyAppLaunched() {
  for (auto& observer : observers_) {
    observer.OnAppLaunched();
  }
}

void KioskAppLauncher::ObserverList::NotifyAppWindowCreated(
    const std::optional<std::string>& app_name) {
  for (auto& observer : observers_) {
    observer.OnAppWindowCreated(app_name);
  }
}

void KioskAppLauncher::ObserverList::NotifyLaunchFailed(
    KioskAppLaunchError::Error error) {
  for (auto& observer : observers_) {
    observer.OnLaunchFailed(error);
  }
}

}  // namespace ash
