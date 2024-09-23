// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/persistent_repeating_timer.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"

namespace dips {

PersistentRepeatingTimer::Storage::~Storage() = default;

PersistentRepeatingTimer::PersistentRepeatingTimer(
    std::unique_ptr<Storage> time_storage,
    base::TimeDelta delay,
    base::RepeatingClosure task)
    : storage_(std::move(time_storage)), delay_(delay), user_task_(task) {}

PersistentRepeatingTimer::~PersistentRepeatingTimer() = default;

void PersistentRepeatingTimer::Start() {
  if (timer_.IsRunning()) {
    return;  // Already started.
  }

  storage_->GetLastFired(
      base::BindOnce(&PersistentRepeatingTimer::StartWithLastFired,
                     weak_factory_.GetWeakPtr()));
}

void PersistentRepeatingTimer::StartWithLastFired(
    std::optional<base::Time> last_fired) {
  if (timer_.IsRunning()) {
    return;  // Already started.
  }

  const base::TimeDelta time_since_update =
      base::Time::Now() - last_fired.value_or(base::Time());
  if (time_since_update >= delay_) {
    OnTimerFired();
  } else {
    timer_.Start(FROM_HERE, delay_ - time_since_update,
                 base::BindRepeating(&PersistentRepeatingTimer::OnTimerFired,
                                     base::Unretained(this)));
  }
  DCHECK(timer_.IsRunning());
}

void PersistentRepeatingTimer::OnTimerFired() {
  DCHECK(!timer_.IsRunning());
  const base::Time now = base::Time::Now();
  storage_->SetLastFired(now);
  user_task_.Run();
  StartWithLastFired(now);
}

}  // namespace dips
