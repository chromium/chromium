// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/bluetooth_metrics_provider.h"

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/floss/floss_dbus_manager.h"
#include "device/bluetooth/floss/floss_features.h"
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

namespace metrics {

BluetoothMetricsProvider::BluetoothMetricsProvider() {
  GetBluetoothAvailability();
}

BluetoothMetricsProvider::~BluetoothMetricsProvider() = default;

void BluetoothMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_WIN)
  base::UmaHistogramEnumeration("Bluetooth.Availability.v2",
                                bluetooth_availability_);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  base::UmaHistogramEnumeration("Bluetooth.StackName",
                                floss::features::IsFlossEnabled()
                                    ? BluetoothStackName::kFloss
                                    : BluetoothStackName::kBlueZ);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) ||
        // BUILDFLAG(IS_WIN)
}

void BluetoothMetricsProvider::OnGetAdapter(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  if (!adapter->IsPresent()) {
    bluetooth_availability_ = BluetoothAvailability::BLUETOOTH_NOT_AVAILABLE;
    return;
  }

  if (!device::BluetoothAdapterFactory::Get()->IsLowEnergySupported()) {
    bluetooth_availability_ =
        BluetoothAvailability::BLUETOOTH_AVAILABLE_WITHOUT_LE;
    return;
  }

  bluetooth_availability_ = BluetoothAvailability::BLUETOOTH_AVAILABLE_WITH_LE;
}

void BluetoothMetricsProvider::GetBluetoothAvailability() {
  // This is only relevant for desktop platforms.
#if BUILDFLAG(IS_MAC)
  // TODO(kenrb): This is separate from other platforms because we get a
  // little bit of extra information from the Mac-specific code. It might not
  // be worth having the extra code path, and we should consider whether to
  // combine them (https://crbug.com/907279).
  bluetooth_availability_ = bluetooth_utility::GetBluetoothAvailability();
#elif BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
  // GetAdapter must be called on the UI thread, because it creates a
  // WeakPtr, which is checked from that thread on future calls.
  if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
    content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
        ->PostTask(
            FROM_HERE,
            base::BindOnce(&BluetoothMetricsProvider::GetBluetoothAvailability,
                           weak_ptr_factory_.GetWeakPtr()));
    return;
  }

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
  bool is_initialized;

  if (floss::features::IsFlossEnabled()) {
    is_initialized = floss::FlossDBusManager::IsInitialized();
  } else {
    is_initialized = bluez::BluezDBusManager::IsInitialized();
  }

  // This is for tests that have not initialized bluez/floss or dbus thread
  // manager. Outside of tests these are initialized earlier during browser
  // startup.
  if (!is_initialized)
    return;
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)

  if (!device::BluetoothAdapterFactory::Get()->IsBluetoothSupported()) {
    bluetooth_availability_ = BluetoothAvailability::BLUETOOTH_NOT_SUPPORTED;
    return;
  }

  device::BluetoothAdapterFactory::Get()->GetAdapter(base::BindOnce(
      &BluetoothMetricsProvider::OnGetAdapter, weak_ptr_factory_.GetWeakPtr()));
#endif
}

}  // namespace metrics
