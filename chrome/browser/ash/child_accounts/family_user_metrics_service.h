// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHILD_ACCOUNTS_FAMILY_USER_METRICS_SERVICE_H_
#define CHROME_BROWSER_ASH_CHILD_ACCOUNTS_FAMILY_USER_METRICS_SERVICE_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/timer/timer.h"
#include "components/keyed_service/core/keyed_service.h"

class PrefRegistrySimple;
class PrefService;

namespace content {
class BrowserContext;
}  // namespace content

namespace ash {

// Service to initialize and control metric recorders of family users on Chrome
// OS.
class FamilyUserMetricsService : public KeyedService {
 public:
  // Interface for observing events on the FamilyUserMetricsService.
  class Observer : public base::CheckedObserver {
   public:
    // Called when we detect a new day. This event can fire sooner or later than
    // 24 hours due to clock or time zone changes.
    virtual void OnNewDay() {}
  };

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);
  // Returns the day id for a given time for testing.
  static int GetDayIdForTesting(base::Time time);

  explicit FamilyUserMetricsService(content::BrowserContext* context);
  FamilyUserMetricsService(const FamilyUserMetricsService&) = delete;
  FamilyUserMetricsService& operator=(const FamilyUserMetricsService&) = delete;
  ~FamilyUserMetricsService() override;

  // KeyedService:
  void Shutdown() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  // Helper function to check if a new day has arrived.
  void CheckForNewDay();

  const raw_ptr<PrefService> pref_service_;

  // A periodic timer that checks if a new day has arrived.
  base::RepeatingTimer timer_;

  // The |observers_| list is a superset of the |family_user_metrics_|.
  base::ObserverList<Observer> observers_;
  std::vector<std::unique_ptr<Observer>> family_user_metrics_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_CHILD_ACCOUNTS_FAMILY_USER_METRICS_SERVICE_H_
