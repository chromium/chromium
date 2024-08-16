// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/components/arc/metrics/arc_daily_metrics.h"

#include <unordered_set>

#include "ash/components/arc/arc_prefs.h"
#include "base/memory/raw_ref.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "components/prefs/pref_service.h"

namespace arc {

namespace {

// DailyObserver is used by ArcDailyMetrics to know when to report metrics.
// Every metrics logging method calls CheckInterval, and if a day has passed,
// all daily metrics are logged. We won't wait too long beyond a day because
// OnLowMemoryKillCounts is called every 10 minutes.
class DailyObserver : public metrics::DailyEvent::Observer {
 public:
  explicit DailyObserver(ArcDailyMetrics& arc_daily_metrics)
      : arc_daily_metrics_(arc_daily_metrics) {}

  DailyObserver(const DailyObserver&) = delete;
  DailyObserver& operator=(const DailyObserver&) = delete;

  ~DailyObserver() override = default;

  // Callback called when the daily event happen.
  void OnDailyEvent(metrics::DailyEvent::IntervalType type) override {
    arc_daily_metrics_->OnDailyEvent(type);
  }

 private:
  const raw_ref<ArcDailyMetrics> arc_daily_metrics_;
};

class KillCounts {
 public:
  KillCounts(const std::string& pref_prefix, const std::string& hist_prefix);
  ~KillCounts() = default;

  void Load(const base::Value::Dict& pref);
  void Save(base::Value::Dict& out_pref);
  void Increment(int oom, int foreground, int perceptible, int cached);
  void UpdateUmaDaily();

 private:
  const std::string pref_prefix_;
  const std::string hist_prefix_;
  int oom_ = 0;
  int foreground_ = 0;
  int perceptible_ = 0;
  int cached_ = 0;
};

KillCounts::KillCounts(const std::string& pref_prefix,
                       const std::string& hist_prefix)
    : pref_prefix_(pref_prefix), hist_prefix_(hist_prefix) {}

void KillCounts::Load(const base::Value::Dict& pref) {
  oom_ = pref.FindInt(pref_prefix_ + "oom").value_or(0);
  foreground_ = pref.FindInt(pref_prefix_ + "foreground").value_or(0);
  perceptible_ = pref.FindInt(pref_prefix_ + "perceptible").value_or(0);
  cached_ = pref.FindInt(pref_prefix_ + "cached").value_or(0);
}

void KillCounts::Save(base::Value::Dict& out_pref) {
  // Only save counter values that are non-zero, to reduce the size of prefs.
  if (oom_ > 0) {
    out_pref.Set(pref_prefix_ + "oom", oom_);
  }
  if (foreground_ > 0) {
    out_pref.Set(pref_prefix_ + "foreground", foreground_);
  }
  if (perceptible_ > 0) {
    out_pref.Set(pref_prefix_ + "perceptible", perceptible_);
  }
  if (cached_ > 0) {
    out_pref.Set(pref_prefix_ + "cached", cached_);
  }
}

void KillCounts::Increment(int oom,
                           int foreground,
                           int perceptible,
                           int cached) {
  oom_ += oom;
  foreground_ += foreground;
  perceptible_ += perceptible;
  cached_ += cached;
}

void KillCounts::UpdateUmaDaily() {
  base::UmaHistogramExactLinear(
      base::StringPrintf("Arc.App.LowMemoryKills%s.OomDaily",
                         hist_prefix_.c_str()),
      oom_, 50);
  base::UmaHistogramExactLinear(
      base::StringPrintf("Arc.App.LowMemoryKills%s.ForegroundDaily",
                         hist_prefix_.c_str()),
      foreground_, 50);
  base::UmaHistogramExactLinear(
      base::StringPrintf("Arc.App.LowMemoryKills%s.PerceptibleDaily",
                         hist_prefix_.c_str()),
      perceptible_, 50);
  base::UmaHistogramExactLinear(
      base::StringPrintf("Arc.App.LowMemoryKills%s.CachedDaily",
                         hist_prefix_.c_str()),
      cached_, 50);

  // Reset the counts for the next day. ArcDailyMetrics is responsible for
  // resetting the cached values in prefs.
  oom_ = 0;
  foreground_ = 0;
  perceptible_ = 0;
  cached_ = 0;
}

}  // namespace

const vm_tools::concierge::VmInfo_VmType
    ArcDailyMetrics::kKillCountTypeVm[ArcDailyMetrics::kKillCountNum] = {
        vm_tools::concierge::VmInfo_VmType_UNKNOWN,    // kKillCountAll not used
        vm_tools::concierge::VmInfo_VmType_UNKNOWN,    // kKillCountOnlyArc not
                                                       // used
        vm_tools::concierge::VmInfo_VmType_TERMINA,    // kKillCountCrostini
        vm_tools::concierge::VmInfo_VmType_PLUGIN_VM,  // kKillCountPluginVm
        vm_tools::concierge::VmInfo_VmType_BOREALIS,   // kKillCountSteam
        vm_tools::concierge::VmInfo_VmType_UNKNOWN,    // kKillCountUnknownVm
};

const char ArcDailyMetrics::kDailyEventHistogramName[] =
    "Arc.DailyEventInterval";

ArcDailyMetrics::ArcDailyMetrics(PrefService* pref_service)
    : prefs_(pref_service),
      daily_event_(
          std::make_unique<metrics::DailyEvent>(pref_service,
                                                prefs::kArcDailyMetricsSample,
                                                kDailyEventHistogramName)),
      kills_{std::make_unique<KillCounts>("", ""),
             std::make_unique<KillCounts>("arc_", ".OnlyArc"),
             std::make_unique<KillCounts>("crostini_", ".Crostini"),
             std::make_unique<KillCounts>("plugin_", ".PluginVm"),
             std::make_unique<KillCounts>("steam_", ".Steam"),
             std::make_unique<KillCounts>("unknown_", ".UnknownVm")} {
  // Restore kill counts.
  const auto& pref = prefs_->GetDict(prefs::kArcDailyMetricsKills);
  for (auto& kill_counts : kills_) {
    kill_counts->Load(pref);
  }

  // Set up daily event.
  daily_event_->AddObserver(std::make_unique<DailyObserver>(*this));
  daily_event_->CheckInterval();
}

ArcDailyMetrics::~ArcDailyMetrics() = default;

void ArcDailyMetrics::OnLowMemoryKillCounts(
    std::optional<vm_tools::concierge::ListVmsResponse> vms_list,
    int oom,
    int foreground,
    int perceptible,
    int cached) {
  // Build set of VMs running now.
  std::unordered_set<vm_tools::concierge::VmInfo_VmType> curr_vms;
  if (vms_list && vms_list->success()) {
    for (int i = 0; i < vms_list->vms_size(); i++) {
      const auto& vm = vms_list->vms(i);
      if (vm.has_vm_info()) {
        const auto& info = vm.vm_info();
        switch (info.vm_type()) {
          case vm_tools::concierge::VmInfo_VmType_ARC_VM:
          case vm_tools::concierge::VmInfo_VmType_BOREALIS:
          case vm_tools::concierge::VmInfo_VmType_PLUGIN_VM:
          case vm_tools::concierge::VmInfo_VmType_TERMINA:
            curr_vms.insert(info.vm_type());
            break;

          default:
            // Map all unknown VMs to VmInfo_VmType_UNKNOWN
            curr_vms.insert(vm_tools::concierge::VmInfo_VmType_UNKNOWN);
            break;
        }

      } else {
        LOG(WARNING) << "VmListToSet got VM " << vm.name()
                     << " with no vm_info.";
      }
    }
  }

  // Which VMs are context for a kill is the union of the VMs running now, and
  //  the VMs running at the end of the last sample.
  std::unordered_set<vm_tools::concierge::VmInfo_VmType> vms;
  vms.insert(curr_vms.begin(), curr_vms.end());
  vms.insert(kills_prev_vms_.begin(), kills_prev_vms_.end());
  kills_prev_vms_ = curr_vms;

  kills_[kKillCountAll]->Increment(oom, foreground, perceptible, cached);

  // If ARCVM is the only VM.
  if (vms.count(vm_tools::concierge::VmInfo_VmType_ARC_VM) != 0 &&
      vms.size() == 1) {
    kills_[kKillCountOnlyArc]->Increment(oom, foreground, perceptible, cached);
  }

  // All background VM counters don't care what other VMs are running.
  for (int i = kKillCountFirstBackgroundVm; i < kKillCountNum; i++) {
    if (vms.count(kKillCountTypeVm[i]) != 0) {
      kills_[i]->Increment(oom, foreground, perceptible, cached);
    }
  }

  // Updating prefs is not free, so skip it if we didn't have any kill counts.
  if (oom != 0 || foreground != 0 || perceptible != 0 || cached != 0) {
    base::Value::Dict pref;
    for (auto& kill_counts : kills_) {
      kill_counts->Save(pref);
    }
    prefs_->SetDict(prefs::kArcDailyMetricsKills, std::move(pref));
  }

  daily_event_->CheckInterval();
}

void ArcDailyMetrics::OnDailyEvent(metrics::DailyEvent::IntervalType type) {
  for (auto& kill_counts : kills_) {
    kill_counts->UpdateUmaDaily();
  }

  // Reset counters.
  prefs_->SetDict(prefs::kArcDailyMetricsKills, base::Value::Dict());
}

void ArcDailyMetrics::SetDailyEventForTesting(
    std::unique_ptr<metrics::DailyEvent> daily_event) {
  daily_event_ = std::move(daily_event);
  daily_event_->AddObserver(std::make_unique<DailyObserver>(*this));
}

}  // namespace arc
