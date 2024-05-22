// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_METRICS_ARC_DAILY_METRICS_H_
#define ASH_COMPONENTS_ARC_METRICS_ARC_DAILY_METRICS_H_

#include <memory>
#include <optional>
#include <string>
#include <unordered_set>

#include "ash/components/arc/mojom/process.mojom.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/vm_concierge/concierge_service.pb.h"
#include "components/metrics/daily_event.h"

class PrefService;

namespace arc {

namespace {
class KillCounts;
}  // namespace

// Keeps track of daily metrics logged by ArcMetricsService, so that values can
// be restored after restarts.
class ArcDailyMetrics {
 public:
  explicit ArcDailyMetrics(PrefService* pref_service);
  ~ArcDailyMetrics();

  ArcDailyMetrics(const ArcDailyMetrics&) = delete;
  ArcDailyMetrics& operator=(const ArcDailyMetrics&) = delete;

  // Receives the number of kills by priority since the last call.
  void OnLowMemoryKillCounts(
      std::optional<vm_tools::concierge::ListVmsResponse> vms_list,
      int oom,
      int foreground,
      int perceptible,
      int cached);

  void OnDailyEvent(metrics::DailyEvent::IntervalType type);

  void SetDailyEventForTesting(
      std::unique_ptr<metrics::DailyEvent> daily_event);

  metrics::DailyEvent* get_daily_event_for_testing() {
    return daily_event_.get();
  }

  // The name of the histogram used to report that the daily event happened.
  static const char kDailyEventHistogramName[];

 private:
  enum KillCountType {
    kKillCountAll,
    kKillCountOnlyArc,

    // Background VMs are all treated the same, so name the first one.
    kKillCountFirstBackgroundVm,
    kKillCountCrostini = kKillCountFirstBackgroundVm,
    kKillCountPluginVm,
    kKillCountSteam,
    kKillCountUnknownVm,

    // Keep count last so we can easily iterate.
    kKillCountNum,
  };

  const raw_ptr<PrefService> prefs_;
  std::unique_ptr<metrics::DailyEvent> daily_event_;

  // Members for tracking Android App kill counts.
  std::unordered_set<vm_tools::concierge::VmInfo_VmType> kills_prev_vms_;
  std::unique_ptr<KillCounts> kills_[kKillCountNum];
  static const vm_tools::concierge::VmInfo_VmType
      kKillCountTypeVm[kKillCountNum];
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_METRICS_ARC_DAILY_METRICS_H_
