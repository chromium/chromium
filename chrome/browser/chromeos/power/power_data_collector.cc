// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/power_data_collector.h"

#include "base/logging.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"

namespace chromeos {

namespace {

// The global PowerDataCollector instance.
PowerDataCollector* g_power_data_collector = NULL;

}  // namespace

const int PowerDataCollector::kSampleTimeLimitSec = 24 * 60 * 60;

// static
void PowerDataCollector::Initialize() {
  // Check that power data collector is initialized only after the
  // PowerManagerClient is initialized.
  CHECK(PowerManagerClient::Get());
  CHECK(g_power_data_collector == NULL);
  g_power_data_collector = new PowerDataCollector(true);
}

void PowerDataCollector::InitializeForTesting() {
  CHECK(PowerManagerClient::Get());
  CHECK(g_power_data_collector == NULL);
  g_power_data_collector = new PowerDataCollector(false);
}

// static
void PowerDataCollector::Shutdown() {
  // Shutdown only if initialized.
  CHECK(g_power_data_collector);
  delete g_power_data_collector;
  g_power_data_collector = NULL;
}

// static
PowerDataCollector* PowerDataCollector::Get() {
  CHECK(g_power_data_collector);
  return g_power_data_collector;
}

void PowerDataCollector::PowerChanged(
    const power_manager::PowerSupplyProperties& prop) {
  PowerSupplySample sample;
  sample.time = base::Time::Now();
  sample.external_power = (prop.external_power() !=
      power_manager::PowerSupplyProperties::DISCONNECTED);
  sample.battery_percent = prop.battery_percent();
  sample.battery_discharge_rate = prop.battery_discharge_rate();
  AddSample(&power_supply_data_, sample);
}

void PowerDataCollector::SuspendDone(const base::TimeDelta& sleep_duration) {
  SystemResumedSample sample;
  sample.time = base::Time::Now();
  sample.sleep_duration = sleep_duration;
  AddSample(&system_resumed_data_, sample);
}

PowerDataCollector::PowerDataCollector(const bool start_cpu_data_collector) {
  PowerManagerClient::Get()->AddObserver(this);
  if (start_cpu_data_collector)
    cpu_data_collector_.Start();
}

PowerDataCollector::~PowerDataCollector() {
  PowerManagerClient::Get()->RemoveObserver(this);
}

PowerDataCollector::PowerSupplySample::PowerSupplySample()
    : external_power(false),
      battery_percent(0.0),
      battery_discharge_rate(0.0) {
}

PowerDataCollector::SystemResumedSample::SystemResumedSample() {
}

}  // namespace chromeos
