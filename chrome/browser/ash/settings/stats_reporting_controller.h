// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SETTINGS_STATS_REPORTING_CONTROLLER_H_
#define CHROME_BROWSER_ASH_SETTINGS_STATS_REPORTING_CONTROLLER_H_

#include "chrome/browser/ash/settings/owner_pending_setting_controller.h"

#include "base/memory/weak_ptr.h"

class PrefRegistrySimple;
class PrefService;

namespace ash {

// Class to control setting of cros.metrics.reportingEnabled device preference
// before ownership is taken.
class StatsReportingController : public OwnerPendingSettingController {
 public:
  // Manage singleton instance.
  static void Initialize(PrefService* local_state);
  static bool IsInitialized();
  static void Shutdown();
  static StatsReportingController* Get();

  StatsReportingController(const StatsReportingController&) = delete;
  StatsReportingController& operator=(const StatsReportingController&) = delete;

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  void SetEnabled(Profile* profile, bool enabled);
  bool IsEnabled() const;

 private:
  explicit StatsReportingController(PrefService* local_state);
  ~StatsReportingController() override;

  base::WeakPtr<StatsReportingController> as_weak_ptr() {
    return weak_factory_.GetWeakPtr();
  }

  base::CallbackListSubscription neutrino_logging_subscription_;

  base::WeakPtrFactory<StatsReportingController> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SETTINGS_STATS_REPORTING_CONTROLLER_H_
