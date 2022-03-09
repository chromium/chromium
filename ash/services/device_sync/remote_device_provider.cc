// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/services/device_sync/remote_device_provider.h"

namespace chromeos {

namespace device_sync {

RemoteDeviceProvider::RemoteDeviceProvider() {}

RemoteDeviceProvider::~RemoteDeviceProvider() {}

void RemoteDeviceProvider::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void RemoteDeviceProvider::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void RemoteDeviceProvider::NotifyObserversDeviceListChanged() {
  for (auto& observer : observers_)
    observer.OnSyncDeviceListChanged();
}

}  // namespace device_sync

}  // namespace chromeos
