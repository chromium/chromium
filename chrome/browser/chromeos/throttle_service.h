// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_THROTTLE_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_THROTTLE_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/throttle_observer.h"

namespace content {
class BrowserContext;
}

namespace chromeos {

// This class is the base for different throttle services on ChromeOS.
// The class holds a number of ThrottleObservers which watch for several
// conditions. When the observers change from active to inactive or vice-versa,
// OnObserverStateChanged finds the active observer with the highest
// PriorityLevel, and calls ThrottleInstance with that level.
class ThrottleService {
 public:
  explicit ThrottleService(content::BrowserContext* context);
  virtual ~ThrottleService();

  // Functions for testing
  void NotifyObserverStateChangedForTesting();
  void SetObserversForTesting(
      std::vector<std::unique_ptr<ThrottleObserver>> observers);
  void set_level_for_testing(ThrottleObserver::PriorityLevel level);

 protected:
  void AddObserver(std::unique_ptr<ThrottleObserver> observer);
  void StartObservers();
  void StopObservers();
  void OnObserverStateChanged();
  void SetLevel(ThrottleObserver::PriorityLevel level);

  // This function is called whenever there is a new level to which the
  // cgroup should be throttled. Derived classes should implement
  // ThrottleInstance to adjust the throttling state of their relevant cgroup.
  virtual void ThrottleInstance(ThrottleObserver::PriorityLevel level) = 0;

  // Whenever there is a change in the effective observer (active observer with
  // the highest PriorityLevel), this function is called with the name of the
  // previously effective observer and the duration it was effective. Derived
  // classes can implement this function to record UMA metrics.
  virtual void RecordCpuRestrictionDisabledUMA(const std::string& observer_name,
                                               base::TimeDelta delta) = 0;

  const std::vector<std::unique_ptr<ThrottleObserver>>& observers() const {
    return observers_;
  }
  content::BrowserContext* context() const { return context_; }
  ThrottleObserver::PriorityLevel level() const { return level_; }

 private:
  content::BrowserContext* const context_;
  std::vector<std::unique_ptr<ThrottleObserver>> observers_;
  ThrottleObserver::PriorityLevel level_{
      ThrottleObserver::PriorityLevel::UNKNOWN};
  ThrottleObserver* last_effective_observer_{nullptr};
  base::TimeTicks last_throttle_transition_;
  base::WeakPtrFactory<ThrottleService> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ThrottleService);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_THROTTLE_SERVICE_H_
