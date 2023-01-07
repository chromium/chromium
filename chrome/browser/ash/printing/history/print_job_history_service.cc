// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/history/print_job_history_service.h"

#include "chrome/browser/ash/printing/history/print_job_history_cleaner.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"

namespace ash {

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

void PrintJobHistoryService::Shutdown() {
  for (auto& observer : observers_) {
    observer.OnShutdown();
  }
}

}  // namespace ash
