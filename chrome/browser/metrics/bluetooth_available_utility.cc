// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/bluetooth_available_utility.h"

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "chrome/browser/mac/bluetooth_utility.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS)

namespace bluetooth_utility {

void ReportAvailability(BluetoothAvailability availability) {
  UMA_HISTOGRAM_ENUMERATION("Bluetooth.Availability.v2", availability,
                            BLUETOOTH_AVAILABILITY_COUNT);
}

void OnGetAdapter(scoped_refptr<device::BluetoothAdapter> adapter) {
  if (!adapter->IsPresent()) {
    ReportAvailability(BLUETOOTH_NOT_AVAILABLE);
    return;
  }

  if (!device::BluetoothAdapterFactory::Get()->IsLowEnergySupported()) {
    ReportAvailability(BLUETOOTH_AVAILABLE_WITHOUT_LE);
    return;
  }

  ReportAvailability(BLUETOOTH_AVAILABLE_WITH_LE);
}

void ReportBluetoothAvailability() {
  // This is only relevant for desktop platforms.
#if defined(OS_MAC)
  // TODO(kenrb): This is separate from other platforms because we get a
  // little bit of extra information from the Mac-specific code. It might not
  // be worth having the extra code path, and we should consider whether to
  // combine them (https://crbug.com/907279).
  ReportAvailability(bluetooth_utility::GetBluetoothAvailability());
#elif defined(OS_WIN) || defined(OS_LINUX) || defined(OS_CHROMEOS)
  // GetAdapter must be called on the UI thread, because it creates a
  // WeakPtr, which is checked from that thread on future calls.
  if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
    content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
        ->PostTask(FROM_HERE, base::BindOnce(&ReportBluetoothAvailability));
    return;
  }

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  // This is for tests that have not initialized bluez or dbus thread manager.
  // Outside of tests these are initialized earlier during browser startup.
  if (!bluez::BluezDBusManager::IsInitialized())
    return;
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS)

  if (!device::BluetoothAdapterFactory::Get()->IsBluetoothSupported()) {
    ReportAvailability(BLUETOOTH_NOT_SUPPORTED);
    return;
  }

  device::BluetoothAdapterFactory::Get()->GetAdapter(
      base::BindOnce(&OnGetAdapter));
#endif
}

}  // namespace bluetooth_utility
