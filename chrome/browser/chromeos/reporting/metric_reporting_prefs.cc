// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/reporting/metric_reporting_prefs.h"

#include "base/check.h"
#include "chrome/browser/chromeos/reporting/metric_default_utils.h"
#include "components/pref_registry/pref_registry_syncable.h"

namespace reporting {

void RegisterProfilePrefs(::user_prefs::PrefRegistrySyncable* registry) {
  CHECK(registry);
  registry->RegisterListPref(kReportWebsiteActivityAllowlist);
  registry->RegisterListPref(kReportWebsiteTelemetryAllowlist);
  registry->RegisterListPref(kReportWebsiteTelemetry);
  registry->RegisterIntegerPref(
      kReportWebsiteTelemetryCollectionRateMs,
      ::reporting::metrics::kDefaultWebsiteTelemetryCollectionRate
          .InMilliseconds());
}

}  // namespace reporting
