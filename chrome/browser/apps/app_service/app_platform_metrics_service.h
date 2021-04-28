// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_PLATFORM_METRICS_SERVICE_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_PLATFORM_METRICS_SERVICE_H_

#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "chrome/browser/profiles/profile.h"

class PrefRegistrySimple;

namespace apps {

extern const char kAppPlatformMetricsDayId[];

// Service to initialize and control app platform metric recorders per day in
// Chrome OS.
class AppPlatformMetricsService {
 public:
  // Interface for observing events on the AppPlatformMetricsService.
  class Observer : public base::CheckedObserver {
   public:
    // Called when we detect a new day.
    virtual void OnNewDay() {}
  };

  explicit AppPlatformMetricsService(Profile* profile);
  AppPlatformMetricsService(const AppPlatformMetricsService&) = delete;
  AppPlatformMetricsService& operator=(const AppPlatformMetricsService&) =
      delete;
  ~AppPlatformMetricsService();

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Returns the day id for a given time for testing.
  static int GetDayIdForTesting(base::Time time);

  // Start the timer and check if a new day has arrived.
  void Start();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  // Helper function to check if a new day has arrived.
  void CheckForNewDay();

  Profile* profile_;

  int day_id_;

  // A periodic timer that checks if a new day has arrived.
  base::RepeatingTimer timer_;

  base::ObserverList<Observer> observers_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_PLATFORM_METRICS_SERVICE_H_
