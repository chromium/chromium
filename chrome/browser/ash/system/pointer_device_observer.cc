// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system/pointer_device_observer.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/ash/system/input_device_settings.h"
#include "content/public/browser/browser_thread.h"
#include "ui/events/devices/device_data_manager.h"

using content::BrowserThread;

namespace ash {
namespace system {

PointerDeviceObserver::PointerDeviceObserver() {}

PointerDeviceObserver::~PointerDeviceObserver() {
  ui::DeviceDataManager::GetInstance()->RemoveObserver(this);
}

void PointerDeviceObserver::Init() {
  ui::DeviceDataManager::GetInstance()->AddObserver(this);
}

void PointerDeviceObserver::CheckDevices() {
  CheckMouseExists();
  CheckPointingStickExists();
  CheckTouchpadExists();
  CheckHapticTouchpadExists();
}

void PointerDeviceObserver::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PointerDeviceObserver::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void PointerDeviceObserver::OnInputDeviceConfigurationChanged(
    uint8_t input_device_types) {
  if (input_device_types & (ui::InputDeviceEventObserver::kMouse |
                            ui::InputDeviceEventObserver::kTouchpad |
                            ui::InputDeviceEventObserver::kPointingStick)) {
    CheckDevices();
  }
}

void PointerDeviceObserver::CheckTouchpadExists() {
  InputDeviceSettings::Get()->TouchpadExists(base::BindOnce(
      &PointerDeviceObserver::OnTouchpadExists, weak_factory_.GetWeakPtr()));
}

void PointerDeviceObserver::CheckHapticTouchpadExists() {
  InputDeviceSettings::Get()->HapticTouchpadExists(
      base::BindOnce(&PointerDeviceObserver::OnHapticTouchpadExists,
                     weak_factory_.GetWeakPtr()));
}

void PointerDeviceObserver::CheckMouseExists() {
  InputDeviceSettings::Get()->MouseExists(base::BindOnce(
      &PointerDeviceObserver::OnMouseExists, weak_factory_.GetWeakPtr()));
}

void PointerDeviceObserver::CheckPointingStickExists() {
  InputDeviceSettings::Get()->PointingStickExists(
      base::BindOnce(&PointerDeviceObserver::OnPointingStickExists,
                     weak_factory_.GetWeakPtr()));
}

void PointerDeviceObserver::OnTouchpadExists(bool exists) {
  for (auto& observer : observers_)
    observer.TouchpadExists(exists);
}

void PointerDeviceObserver::OnHapticTouchpadExists(bool exists) {
  for (auto& observer : observers_)
    observer.HapticTouchpadExists(exists);
}

void PointerDeviceObserver::OnMouseExists(bool exists) {
  for (auto& observer : observers_)
    observer.MouseExists(exists);
}

void PointerDeviceObserver::OnPointingStickExists(bool exists) {
  for (auto& observer : observers_)
    observer.PointingStickExists(exists);
}

PointerDeviceObserver::Observer::~Observer() {
}

}  // namespace system
}  // namespace ash
