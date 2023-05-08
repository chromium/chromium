// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/metric_reporting_prefs.h"

#include "base/values.h"
#include "chrome/browser/chromeos/reporting/metric_default_utils.h"
#include "components/pref_registry/pref_registry_syncable.h"

namespace ash::reporting {

void RegisterProfilePrefs(::user_prefs::PrefRegistrySyncable* registry) {
  DCHECK(registry);
  registry->RegisterListPref(kReportAppInventory);
  registry->RegisterListPref(kReportAppUsage);
  registry->RegisterIntegerPref(
      kReportAppUsageCollectionRateMs,
      ::reporting::metrics::kDefaultAppUsageTelemetryCollectionRate
          .InMilliseconds());
}

}  // namespace ash::reporting
