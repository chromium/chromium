// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/prefetch/prefetch_background_task_handler_impl.h"

#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "chrome/browser/offline_pages/prefetch/prefetch_background_task_scheduler.h"
#include "chrome/common/pref_names.h"
#include "components/offline_pages/core/offline_clock.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/offline_pages/core/prefetch/prefetch_prefs.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "net/base/backoff_entry_serializer.h"

namespace offline_pages {

namespace {
const int kDefaultSuspensionDays = 1;
const net::BackoffEntry::Policy kPrefetchBackoffPolicy = {
    0,                 // Number of initial errors to ignore without backoff.
    30 * 1000,         // Initial delay for backoff in ms: 30 seconds.
    2,                 // Factor to multiply for exponential backoff.
    0.33,              // Fuzzing percentage.
    24 * 3600 * 1000,  // Maximum time to delay requests in ms: 1 day.
    -1,                // Don't discard entry even if unused.
    false              // Don't use initial delay unless the last was an error.
};
}  // namespace

PrefetchBackgroundTaskHandlerImpl::PrefetchBackgroundTaskHandlerImpl(
    PrefService* prefs)
    : prefs_(prefs), tick_clock_(base::DefaultTickClock::GetInstance()) {}
PrefetchBackgroundTaskHandlerImpl::~PrefetchBackgroundTaskHandlerImpl() =
    default;

void PrefetchBackgroundTaskHandlerImpl::CancelBackgroundTask() {
  PrefetchBackgroundTaskScheduler::Cancel();
}

void PrefetchBackgroundTaskHandlerImpl::EnsureTaskScheduled() {
  if (prefetch_prefs::IsLimitlessPrefetchingEnabled(prefs_)) {
    PrefetchBackgroundTaskScheduler::ScheduleLimitless(
        GetAdditionalBackoffSeconds());
  } else {
    PrefetchBackgroundTaskScheduler::Schedule(GetAdditionalBackoffSeconds());
  }
}

int PrefetchBackgroundTaskHandlerImpl::GetAdditionalBackoffSeconds() const {
  return static_cast<int>(
      GetCurrentBackoff()->GetTimeUntilRelease().InSeconds());
}

std::unique_ptr<net::BackoffEntry>
PrefetchBackgroundTaskHandlerImpl::GetCurrentBackoff() const {
  const base::Value::List& value = prefs_->GetList(prefetch_prefs::kBackoff);
  std::unique_ptr<net::BackoffEntry> result =
      net::BackoffEntrySerializer::DeserializeFromList(
          value, &kPrefetchBackoffPolicy, tick_clock_, OfflineTimeNow());
  if (!result)
    return std::make_unique<net::BackoffEntry>(&kPrefetchBackoffPolicy,
                                               tick_clock_);
  return result;
}

void PrefetchBackgroundTaskHandlerImpl::Backoff() {
  std::unique_ptr<net::BackoffEntry> current = GetCurrentBackoff();
  current->InformOfRequest(false);
  UpdateBackoff(current.get());
}

void PrefetchBackgroundTaskHandlerImpl::ResetBackoff() {
  std::unique_ptr<net::BackoffEntry> current = GetCurrentBackoff();
  current->Reset();
  UpdateBackoff(current.get());
}

void PrefetchBackgroundTaskHandlerImpl::PauseBackoffUntilNextRun() {
  std::unique_ptr<net::BackoffEntry> current = GetCurrentBackoff();
  // Erase the existing delay but retain the failure count so that the next
  // time we run if backoff is requested again we will continue the exponential
  // backoff from where we left off.
  current->SetCustomReleaseTime(tick_clock_->NowTicks());
  UpdateBackoff(current.get());
}

void PrefetchBackgroundTaskHandlerImpl::Suspend() {
  std::unique_ptr<net::BackoffEntry> current = GetCurrentBackoff();
  // Set the failure count to 0.
  current->Reset();
  // Set a custom delay to be a 1 day interval. After the day passes, the next
  // backoff value will be back to the initial 30s delay.
  current->SetCustomReleaseTime(tick_clock_->NowTicks() +
                                base::Days(kDefaultSuspensionDays));
  UpdateBackoff(current.get());
}

void PrefetchBackgroundTaskHandlerImpl::RemoveSuspension() {
  std::unique_ptr<net::BackoffEntry> current = GetCurrentBackoff();
  // Reset the backoff completely, but only if the failure count is 0 and there
  // is a custom delay. This should only happen after Suspend() has been called.
  if (current->failure_count() == 0 && !current->GetReleaseTime().is_null())
    current->Reset();
  UpdateBackoff(current.get());
}

void PrefetchBackgroundTaskHandlerImpl::SetTickClockForTesting(
    const base::TickClock* tick_clock) {
  tick_clock_ = tick_clock;
}

void PrefetchBackgroundTaskHandlerImpl::UpdateBackoff(
    net::BackoffEntry* backoff) {
  base::Value::List serialized =
      net::BackoffEntrySerializer::SerializeToList(*backoff, OfflineTimeNow());
  prefs_->SetList(prefetch_prefs::kBackoff, std::move(serialized));
}

}  // namespace offline_pages
