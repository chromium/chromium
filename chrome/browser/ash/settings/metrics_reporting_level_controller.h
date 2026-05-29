// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SETTINGS_METRICS_REPORTING_LEVEL_CONTROLLER_H_
#define CHROME_BROWSER_ASH_SETTINGS_METRICS_REPORTING_LEVEL_CONTROLLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/settings/owner_pending_setting_controller.h"
#include "components/metrics/metrics_reporting_level.h"

class PrefRegistrySimple;
class PrefService;

namespace ash {

// Class to control setting of cros.metrics.reportingLevel device preference
// before ownership is taken.
class MetricsReportingLevelController : public OwnerPendingSettingController {
 public:
  // Manage singleton instance.
  static void Initialize(PrefService* local_state);
  static bool IsInitialized();
  static void Shutdown();
  static MetricsReportingLevelController* Get();

  MetricsReportingLevelController(const MetricsReportingLevelController&) =
      delete;
  MetricsReportingLevelController& operator=(
      const MetricsReportingLevelController&) = delete;

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  void SetLevel(Profile* profile, metrics::MetricsReportingLevel level);
  metrics::MetricsReportingLevel GetLevel() const;

 private:
  explicit MetricsReportingLevelController(PrefService* local_state);
  ~MetricsReportingLevelController() override;

  base::WeakPtr<MetricsReportingLevelController> as_weak_ptr() {
    return weak_factory_.GetWeakPtr();
  }

  base::WeakPtrFactory<MetricsReportingLevelController> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SETTINGS_METRICS_REPORTING_LEVEL_CONTROLLER_H_
