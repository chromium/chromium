// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/persistent_repeating_timer.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/prefs/pref_service.h"

namespace dips {

namespace {

// TODO: crbug.com/328209292 - Remove this class.
class PrefsStorage : public PersistentRepeatingTimer::Storage {
 public:
  PrefsStorage(PrefService* prefs, const char* pref_name);
  ~PrefsStorage() override;

  void GetLastFired(TimeCallback callback) const override;
  void SetLastFired(base::Time time) override;

 private:
  raw_ptr<PrefService> prefs_;
  std::string pref_name_;
};

PrefsStorage::PrefsStorage(PrefService* prefs, const char* pref_name)
    : prefs_(prefs), pref_name_(pref_name) {}

PrefsStorage::~PrefsStorage() = default;

void PrefsStorage::GetLastFired(TimeCallback callback) const {
  std::optional<base::Time> time;
  if (prefs_->HasPrefPath(pref_name_)) {
    time = prefs_->GetTime(pref_name_);
  }
  std::move(callback).Run(time);
}

void PrefsStorage::SetLastFired(base::Time time) {
  prefs_->SetTime(pref_name_, time);
}

}  // namespace

PersistentRepeatingTimer::Storage::~Storage() = default;

PersistentRepeatingTimer::PersistentRepeatingTimer(
    PrefService* prefs,
    const char* timer_last_update_pref_name,
    base::TimeDelta delay,
    base::RepeatingClosure task)
    : PersistentRepeatingTimer(
          std::make_unique<PrefsStorage>(prefs, timer_last_update_pref_name),
          delay,
          task) {}

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
