// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/usage_scenario/system_event_provider.h"

#include "base/power_monitor/power_monitor.h"
#include "chrome/browser/metrics/usage_scenario/usage_scenario_data_store.h"

SystemEventProvider::SystemEventProvider(UsageScenarioDataStoreImpl* data_store)
    : data_store_(data_store) {
  if (auto* power_monitor = base::PowerMonitor::GetInstance();
      power_monitor->IsInitialized()) {
    power_monitor->AddPowerSuspendObserver(this);
  }
}

SystemEventProvider::~SystemEventProvider() {
  if (auto* power_monitor = base::PowerMonitor::GetInstance();
      power_monitor->IsInitialized()) {
    power_monitor->RemovePowerSuspendObserver(this);
  }
}

void SystemEventProvider::OnSuspend() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  data_store_->OnSleepEvent();
}
