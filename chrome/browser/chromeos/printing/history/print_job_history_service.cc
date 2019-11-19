// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/history/print_job_history_service.h"
#include "chrome/browser/chromeos/printing/history/print_job_history_cleaner.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"

namespace chromeos {

PrintJobHistoryService::PrintJobHistoryService() = default;

PrintJobHistoryService::~PrintJobHistoryService() = default;

// static
void PrintJobHistoryService::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(
      prefs::kPrintJobHistoryExpirationPeriod,
      PrintJobHistoryCleaner::kDefaultPrintJobHistoryExpirationPeriodDays);
}

void PrintJobHistoryService::AddObserver(
    PrintJobHistoryService::Observer* observer) {
  observers_.AddObserver(observer);
}

void PrintJobHistoryService::RemoveObserver(
    PrintJobHistoryService::Observer* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace chromeos
