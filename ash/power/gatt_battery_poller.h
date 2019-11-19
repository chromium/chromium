// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_POWER_GATT_BATTERY_POLLER_H_
#define ASH_POWER_GATT_BATTERY_POLLER_H_

#include <cstdint>
#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"

namespace base {
class OneShotTimer;
}  // namespace base

namespace device {
class BluetoothAdapter;
}  // namespace device

namespace ash {

class GattBatteryPercentageFetcher;

// Gets the battery level of a connected Bluetooth device through the
// standardized GATT Battery Service. Polling is done periodically and updates
// the device::BluetoothDevice with the specified |device_address_|.
class ASH_EXPORT GattBatteryPoller {
 public:
  class Factory {
   public:
    virtual ~Factory() = default;

    static void SetFactoryForTesting(Factory* factory);
    static std::unique_ptr<GattBatteryPoller> NewInstance(
        scoped_refptr<device::BluetoothAdapter> adapter,
        const std::string& device_address,
        std::unique_ptr<base::OneShotTimer> poll_timer);

    virtual std::unique_ptr<GattBatteryPoller> BuildInstance(
        scoped_refptr<device::BluetoothAdapter> adapter,
        const std::string& device_address,
        std::unique_ptr<base::OneShotTimer> poll_timer) = 0;
  };

  virtual ~GattBatteryPoller();

 protected:
  GattBatteryPoller(const std::string& device_address);

  // Calling this function starts the fetching process. This allows tests to
  // to create instances of this class without running the whole mechanism.
  void StartFetching(scoped_refptr<device::BluetoothAdapter> adapter,
                     std::unique_ptr<base::OneShotTimer> poll_timer);

 private:
  friend class GattBatteryPollerTest;

  // A GattBatteryPercentageFetcher object is created every time it is needed to
  // read the battery level.
  void CreateBatteryFetcher();

  // Callback function to run after the fetcher completes getting the battery.
  void OnBatteryPercentageFetched(base::Optional<uint8_t> battery_percentage);

  // Schedules the next attempt for reading the battery level. Will stop
  // scheduling after several consecutive unsuccessful attempts or if the device
  // is no longer found in the adapter.
  void ScheduleNextAttempt(bool was_last_attempt_successful);

  // Starts a timer. When time's up, tries to fetch the battery level again.
  void StartNextAttemptTimer();

  scoped_refptr<device::BluetoothAdapter> adapter_;
  const std::string device_address_;
  std::unique_ptr<base::OneShotTimer> poll_timer_;
  std::unique_ptr<GattBatteryPercentageFetcher> fetcher_;

  // The number of consecutive attempts we have tried to read the battery status
  // and failed. Resets when the battery is read successfully.
  int current_retry_count_ = 0;

  base::WeakPtrFactory<GattBatteryPoller> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(GattBatteryPoller);
};

}  // namespace ash

#endif  // ASH_POWER_GATT_BATTERY_POLLER_H_
