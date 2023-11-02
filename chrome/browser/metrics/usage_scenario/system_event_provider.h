// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_USAGE_SCENARIO_SYSTEM_EVENT_PROVIDER_H_
#define CHROME_BROWSER_METRICS_USAGE_SCENARIO_SYSTEM_EVENT_PROVIDER_H_

#include "base/memory/raw_ptr.h"
#include "base/power_monitor/power_observer.h"
#include "base/sequence_checker.h"

class UsageScenarioDataStoreImpl;

// Provides events related to the system state.
class SystemEventProvider : public base::PowerSuspendObserver {
 public:
  explicit SystemEventProvider(UsageScenarioDataStoreImpl* data_store);
  ~SystemEventProvider() override;

  SystemEventProvider(const SystemEventProvider& rhs) = delete;
  SystemEventProvider& operator=(const SystemEventProvider& rhs) = delete;

  // base::PowerSuspendObserver:
  void OnSuspend() override;

 private:
  // The data store, must outlive |this|.
  const raw_ptr<UsageScenarioDataStoreImpl> data_store_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // CHROME_BROWSER_METRICS_USAGE_SCENARIO_SYSTEM_EVENT_PROVIDER_H_
