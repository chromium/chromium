// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/one_time_permissions_condition_tracker.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

OneTimePermissionsConditionTracker::Factory::Factory(
    base::RepeatingCallback<void(const url::Origin&)>
        on_all_references_released,
    base::TimeDelta delay)
    : on_all_references_released_(std::move(on_all_references_released)),
      delay_(delay) {}

OneTimePermissionsConditionTracker::Factory::~Factory() = default;

scoped_refptr<OneTimePermissionsConditionTracker>
OneTimePermissionsConditionTracker::Factory::New(const url::Origin& origin) {
  auto it = map_.find(origin);
  if (it != map_.end()) {
    return base::WrapRefCounted(it->second);
  }
  timers_map_.erase(origin);
  auto new_entry =
      base::MakeRefCounted<OneTimePermissionsConditionTracker>(base::BindOnce(
          &Factory::OnTrackerDestroyed, weak_factory_.GetWeakPtr(), origin));
  map_[origin] = new_entry.get();
  return new_entry;
}

void OneTimePermissionsConditionTracker::Factory::OnTrackerDestroyed(
    const url::Origin& origin) {
  // Erase the map entry first to ensure we don't have a dangling pointer.
  map_.erase(origin);

  std::unique_ptr<base::OneShotTimer> timer =
      std::make_unique<base::OneShotTimer>();
  if (task_runner_) {
    timer->SetTaskRunner(task_runner_);
  }
  timer->Start(FROM_HERE, delay_,
               base::BindOnce(on_all_references_released_, origin)
                   .Then(base::BindOnce(&Factory::EraseTimer,
                                        weak_factory_.GetWeakPtr(), origin)));
  timers_map_[origin] = std::move(timer);
}

void OneTimePermissionsConditionTracker::Factory::EraseTimer(
    const url::Origin& origin) {
  auto it = timers_map_.find(origin);
  if (it != timers_map_.end() && !it->second->IsRunning()) {
    timers_map_.erase(it);
  }
}

void OneTimePermissionsConditionTracker::Factory::SetTaskRunnerForTesting(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  task_runner_ = std::move(task_runner);
}

OneTimePermissionsConditionTracker::OneTimePermissionsConditionTracker(
    base::OnceClosure on_destruction)
    : on_destruction_(std::move(on_destruction)) {}

OneTimePermissionsConditionTracker::~OneTimePermissionsConditionTracker() {
  if (on_destruction_) {
    std::move(on_destruction_).Run();
  }
}
