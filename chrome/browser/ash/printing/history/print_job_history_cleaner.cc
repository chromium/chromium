// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/history/print_job_history_cleaner.h"

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "chrome/browser/ash/printing/history/print_job_info.pb.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace ash {

namespace {

// This "PrintJobHistoryExpirationPeriod" policy value stands for storing the
// print job history indefinitely.
constexpr int kPrintJobHistoryIndefinite = -1;

// Returns true if |pref_service| has been initialized.
bool IsPrefServiceInitialized(PrefService* pref_service) {
  return pref_service->GetAllPrefStoresInitializationStatus() !=
         PrefService::INITIALIZATION_STATUS_WAITING;
}

bool IsCompletionTimeExpired(
    base::Time completion_time,
    base::Time now,
    base::TimeDelta print_job_history_expiration_period) {
  return completion_time + print_job_history_expiration_period < now;
}

}  // namespace

PrintJobHistoryCleaner::PrintJobHistoryCleaner(
    PrintJobDatabase* print_job_database,
    PrefService* pref_service)
    : print_job_database_(print_job_database),
      pref_service_(pref_service),
      clock_(base::DefaultClock::GetInstance()) {}

PrintJobHistoryCleaner::~PrintJobHistoryCleaner() = default;

void PrintJobHistoryCleaner::CleanUp(base::OnceClosure callback) {
  if (IsPrefServiceInitialized(pref_service_)) {
    OnPrefServiceInitialized(std::move(callback), true);
    return;
  }
  // Register for a callback that will be invoked when |pref_service_| is
  // initialized.
  pref_service_->AddPrefInitObserver(
      base::BindOnce(&PrintJobHistoryCleaner::OnPrefServiceInitialized,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void PrintJobHistoryCleaner::SetClockForTesting(const base::Clock* clock) {
  clock_ = clock;
}

void PrintJobHistoryCleaner::OnPrefServiceInitialized(
    base::OnceClosure callback,
    bool success) {
  int expiration_period =
      pref_service_->GetInteger(prefs::kPrintJobHistoryExpirationPeriod);

  // We don't want to run cleanup procedure if there are no expired print jobs.
  if (!success || !print_job_database_->IsInitialized() ||
      expiration_period == kPrintJobHistoryIndefinite ||
      !IsCompletionTimeExpired(oldest_print_job_completion_time_, clock_->Now(),
                               base::Days(expiration_period))) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
    return;
  }
  print_job_database_->GetPrintJobs(
      base::BindOnce(&PrintJobHistoryCleaner::OnPrintJobsRetrieved,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void PrintJobHistoryCleaner::OnPrintJobsRetrieved(
    base::OnceClosure callback,
    bool success,
    std::vector<printing::proto::PrintJobInfo> print_job_infos) {
  if (!success) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
    return;
  }
  std::vector<std::string> print_job_ids_to_remove;
  base::TimeDelta print_job_history_expiration_period = base::Days(
      pref_service_->GetInteger(prefs::kPrintJobHistoryExpirationPeriod));

  base::Time now = clock_->Now();
  oldest_print_job_completion_time_ = now;
  for (const auto& print_job_info : print_job_infos) {
    base::Time completion_time = base::Time::FromMillisecondsSinceUnixEpoch(
        print_job_info.completion_time());
    if (IsCompletionTimeExpired(completion_time, now,
                                print_job_history_expiration_period)) {
      print_job_ids_to_remove.push_back(print_job_info.id());
    } else if (completion_time < oldest_print_job_completion_time_) {
      oldest_print_job_completion_time_ = completion_time;
    }
  }
  print_job_database_->DeletePrintJobs(
      print_job_ids_to_remove,
      base::BindOnce(&PrintJobHistoryCleaner::OnPrintJobsDeleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void PrintJobHistoryCleaner::OnPrintJobsDeleted(base::OnceClosure callback,
                                                bool success) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                           std::move(callback));
}

}  // namespace ash
