// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/power/gatt_battery_poller.h"

#include "ash/power/gatt_battery_percentage_fetcher.h"
#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace ash {

namespace {

// Maximum number of consecutive attempts to try reading the battery status.
// The class stops polling if |current_retry_count_| exceeds this value.
const int kMaxRetryCount = 3;

// Default interval for polling the device battery value.
constexpr base::TimeDelta kDefaultPollInterval =
    base::TimeDelta::FromMinutes(10);

GattBatteryPoller::Factory* g_test_factory_instance = nullptr;

}  // namespace

// static
void GattBatteryPoller::Factory::SetFactoryForTesting(Factory* factory) {
  g_test_factory_instance = factory;
}

// static
std::unique_ptr<GattBatteryPoller> GattBatteryPoller::Factory::NewInstance(
    scoped_refptr<device::BluetoothAdapter> adapter,
    const std::string& device_address,
    std::unique_ptr<base::OneShotTimer> poll_timer) {
  if (g_test_factory_instance) {
    return g_test_factory_instance->BuildInstance(adapter, device_address,
                                                  std::move(poll_timer));
  }
  auto instance = base::WrapUnique(new GattBatteryPoller(device_address));
  instance->StartFetching(adapter, std::move(poll_timer));
  return instance;
}

GattBatteryPoller::GattBatteryPoller(const std::string& device_address)
    : device_address_(device_address) {}

GattBatteryPoller::~GattBatteryPoller() = default;

void GattBatteryPoller::StartFetching(
    scoped_refptr<device::BluetoothAdapter> adapter,
    std::unique_ptr<base::OneShotTimer> poll_timer) {
  adapter_ = adapter;
  poll_timer_ = std::move(poll_timer);
  CreateBatteryFetcher();
}

void GattBatteryPoller::CreateBatteryFetcher() {
  DCHECK(!fetcher_);
  // Creating the fetcher implicitly begins the process of fetching the battery
  // status.
  fetcher_ = GattBatteryPercentageFetcher::Factory::NewInstance(
      adapter_, device_address_,
      base::BindOnce(&GattBatteryPoller::OnBatteryPercentageFetched,
                     weak_ptr_factory_.GetWeakPtr()));
}

void GattBatteryPoller::OnBatteryPercentageFetched(
    base::Optional<uint8_t> battery_percentage) {
  fetcher_.reset();

  if (battery_percentage) {
    device::BluetoothDevice* device = adapter_->GetDevice(device_address_);
    if (device)
      device->SetBatteryPercentage(*battery_percentage);
  }

  ScheduleNextAttempt(battery_percentage.has_value());
}

void GattBatteryPoller::ScheduleNextAttempt(bool was_last_attempt_successful) {
  device::BluetoothDevice* device = adapter_->GetDevice(device_address_);
  // If the device is not present now, it won't be present in the future. Give
  // up retrying.
  if (!device)
    return;

  if (was_last_attempt_successful) {
    current_retry_count_ = 0;
    StartNextAttemptTimer();
    return;
  }

  ++current_retry_count_;
  if (current_retry_count_ <= kMaxRetryCount) {
    StartNextAttemptTimer();
  } else {
    // Reset battery field after exceeding the retry count.
    device->SetBatteryPercentage(base::nullopt);
  }
}

void GattBatteryPoller::StartNextAttemptTimer() {
  poll_timer_->Start(FROM_HERE, kDefaultPollInterval,
                     base::BindOnce(&GattBatteryPoller::CreateBatteryFetcher,
                                    weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace ash
