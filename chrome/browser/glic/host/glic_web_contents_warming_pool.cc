// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_web_contents_warming_pool.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory_coordinator/utils.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/glic/host/glic_no_webview_contents_manager.h"
#include "chrome/browser/glic/host/glic_web_client_manager.h"
#include "chrome/browser/glic/host/glic_web_contents_manager.h"
#include "chrome/browser/glic/host/glic_webui_contents_manager.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/web_contents.h"

namespace glic {

BASE_FEATURE(kGlicReloadWebContentsAfterExpiry,
             base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<int> kGlicMaxReloadCount{
    &kGlicReloadWebContentsAfterExpiry, "max_reload_count", 4};

namespace {

GlicWebContentsWarmingPool::ContainerCreationReason ToContainerCreationReason(
    GlicWarmingTrigger trigger) {
  switch (trigger) {
    case GlicWarmingTrigger::kStartup:
      return GlicWebContentsWarmingPool::ContainerCreationReason::
          kInitialColdWarming;
    case GlicWarmingTrigger::kNudge:
      return GlicWebContentsWarmingPool::ContainerCreationReason::kNudge;
    case GlicWarmingTrigger::kIph:
      return GlicWebContentsWarmingPool::ContainerCreationReason::kIph;
  }
}
}  // namespace

class GlicWebContentsWarmingPool::Metrics {
 public:
  using WarmedContainerFate = GlicWebContentsWarmingPool::WarmedContainerFate;

  void RecordWarmingBlockedByMemoryPressure() {
    miss_status_ = WarmingPoolStatus::kMemoryPressure;
  }

  void OnReloadAfterExpiry(
      GlicWebContentsWarmingPool::ReloadAfterExpiryStatus status) {
    base::UmaHistogramEnumeration("Glic.WarmingPool.ReloadAfterExpiry", status);
  }

  void OnWarmedContentCreated(
      GlicWebContentsWarmingPool::ContainerCreationReason reason) {
    base::UmaHistogramEnumeration("Glic.WarmingPool.ContainerCreationReason",
                                  reason);
    warmed_container_creation_time_ = base::TimeTicks::Now();
    miss_status_ = WarmingPoolStatus::kCold;
  }

  void RecordWarmedContainerFate(WarmedContainerFate fate) {
    base::UmaHistogramEnumeration("Glic.WarmingPool.WarmedContainerFate", fate);
  }

  GlicWebContentsWarmingPool::WarmingPoolStatus RecordTakeContainerStatus(
      const std::unique_ptr<GlicWebContentsManager>& warmed_container,
      bool has_pending_backfill) {
    WarmingPoolStatus status = WarmingPoolStatus::kCold;
    if (warmed_container) {
      status = warmed_container->ShouldReloadOnShow()
                   ? WarmingPoolStatus::kCrashed
                   : WarmingPoolStatus::kHit;
      if (status == WarmingPoolStatus::kHit) {
        RecordWarmedContainerFate(WarmedContainerFate::kUsed);
      }
    } else if (has_pending_backfill) {
      status = WarmingPoolStatus::kPendingBackfill;
    } else {
      status = miss_status_;
    }

    base::UmaHistogramEnumeration("Glic.WarmingPool.HitStatus", status);
    RecordTimeSinceCreatedAt(status);

    miss_status_ = WarmingPoolStatus::kCold;
    return status;
  }

  void RecordClear(ClearReason reason,
                   bool had_container,
                   bool had_pending_backfill) {
    if (had_container) {
      const WarmedContainerFate fate = [reason]() {
        switch (reason) {
          case ClearReason::kShutdown:
            return WarmedContainerFate::kDeletedOnChromeClosed;
          case ClearReason::kMemoryPressure:
            return WarmedContainerFate::kDeletedOnMemoryPressure;
          case ClearReason::kExpired:
            return WarmedContainerFate::kExpired;
        }
      }();
      RecordWarmedContainerFate(fate);
    }
    if (reason == ClearReason::kMemoryPressure &&
        (had_container || had_pending_backfill)) {
      miss_status_ = WarmingPoolStatus::kMemoryPressure;
    } else if (reason == ClearReason::kExpired) {
      miss_status_ = WarmingPoolStatus::kExpired;
    }
  }

 private:
  void RecordTimeSinceCreatedAt(
      GlicWebContentsWarmingPool::WarmingPoolStatus status) {
    if (!warmed_container_creation_time_.has_value()) {
      return;
    }
    const char* histogram_name =
        status == GlicWebContentsWarmingPool::WarmingPoolStatus::kHit
            ? "Glic.WarmingPool.TimeSinceCreatedAtHit"
            : "Glic.WarmingPool.TimeSinceCreatedAtMiss";
    base::UmaHistogramLongTimes(
        histogram_name,
        base::TimeTicks::Now() - *warmed_container_creation_time_);
    warmed_container_creation_time_.reset();
  }

  // Reason for a miss if warmed_container_ is not present when taken.
  WarmingPoolStatus miss_status_ = WarmingPoolStatus::kCold;

  // Creation time of the warmed_container_. For misses, this is preserved
  // from the most recently destroyed container until reported.
  std::optional<base::TimeTicks> warmed_container_creation_time_;
};

GlicWebContentsWarmingPool::GlicWebContentsWarmingPool(Profile* profile,
                                                       GlicEnabling* enabling)
    : profile_(profile),
      enabling_(enabling),
      backfill_scheduler_(GlicWarmingScheduler::Options{
          .use_performance_manager = base::FeatureList::IsEnabled(
              features::kGlicBackfillWarmingUsePerformanceManager),
          .delay =
              base::FeatureList::IsEnabled(features::kGlicWebContentsWarming)
                  ? features::kGlicWebContentsWarmingDelay.Get()
                  : base::Seconds(20),
      }),
      metrics_(std::make_unique<Metrics>()) {
  CHECK(enabling_);
  profile_observation_.Observe(profile_);
  if (base::FeatureList::IsEnabled(features::kGlicWebContentsWarming)) {
    expiry_delay_ = features::kGlicWebContentsWarmingPoolExpiryDelay.Get();
  }
}

GlicWebContentsWarmingPool::~GlicWebContentsWarmingPool() {
  Shutdown();
}

std::unique_ptr<GlicWebContentsManager>
GlicWebContentsWarmingPool::TakeContainer() {
  metrics_->RecordTakeContainerStatus(
      warmed_container_,
      /*has_pending_backfill=*/backfill_scheduler_.IsScheduled());
  reload_count_ = 0;
  should_warm_when_memory_allows_ = true;

  EnsurePreload(ContainerCreationReason::kUserTriggeredColdStart);
  std::unique_ptr<GlicWebContentsManager> result = std::move(warmed_container_);
  warmed_container_ = nullptr;
  expiry_timer_.Stop();

  EnsurePreloadDelayed(ContainerCreationReason::kRefill);
  return result;
}

bool GlicWebContentsWarmingPool::MaybeStartWarming(GlicWarmingTrigger trigger) {
  if (profile_->ShutdownStarted()) {
    return false;
  }
  should_warm_when_memory_allows_ = true;
  if (IsUnderMemoryPressure()) {
    metrics_->RecordWarmingBlockedByMemoryPressure();
    return false;
  }
  EnsurePreload(ToContainerCreationReason(trigger));
  return true;
}

void GlicWebContentsWarmingPool::Shutdown() {
  profile_observation_.Reset();
  should_warm_when_memory_allows_ = false;
  Clear(ClearReason::kShutdown);
}

void GlicWebContentsWarmingPool::OnProfileWillBeDestroyed(Profile* profile) {
  Shutdown();
}

std::unique_ptr<GlicWebContentsManager>
GlicWebContentsWarmingPool::CreateContainer() {
  TRACE_EVENT("glic", "GlicWebContentsWarmingPool::CreateContainer");
  bool initially_hidden =
      base::FeatureList::IsEnabled(features::kGlicContentsInitiallyHidden);
  if (features::IsGlicNoWebviewEnabled()) {
    return std::make_unique<GlicNoWebviewContentsManager>(profile_, enabling_,
                                                          initially_hidden);
  }
  return std::make_unique<GlicWebUIContentsManager>(profile_, initially_hidden);
}

void GlicWebContentsWarmingPool::Clear(ClearReason reason) {
  metrics_->RecordClear(
      reason, /*had_container=*/!!warmed_container_,
      /*had_pending_backfill=*/backfill_scheduler_.IsScheduled());
  warmed_container_.reset();
  backfill_scheduler_.Cancel();
  expiry_timer_.Stop();
}

void GlicWebContentsWarmingPool::OnContainerExpired() {
  CHECK(warmed_container_);
  TRACE_EVENT_INSTANT("glic", "GlicWebContentsWarmingPool::OnContainerExpired");
  Clear(ClearReason::kExpired);
  if (IsUnderMemoryPressure()) {
    return;
  }
  // This only happens if there was a warmed contents at the time of expiry.
  // If the warmed contents had been removed because of memory pressure or
  // some other mechanism, we wouldn't rewarm.
  if (base::FeatureList::IsEnabled(kGlicReloadWebContentsAfterExpiry)) {
    if (reload_count_ < kGlicMaxReloadCount.Get()) {
      reload_count_++;
      metrics_->OnReloadAfterExpiry(
          GlicWebContentsWarmingPool::ReloadAfterExpiryStatus::kReloaded);
      EnsurePreload(ContainerCreationReason::kReloadAfterExpiry);
    } else {
      should_warm_when_memory_allows_ = false;
      metrics_->OnReloadAfterExpiry(
          GlicWebContentsWarmingPool::ReloadAfterExpiryStatus::
              kNotReloadedLimitReached);
    }
  } else {
    should_warm_when_memory_allows_ = false;
    metrics_->OnReloadAfterExpiry(
        GlicWebContentsWarmingPool::ReloadAfterExpiryStatus::
            kNotReloadedFeatureDisabled);
  }
}

void GlicWebContentsWarmingPool::EnsurePreload(ContainerCreationReason reason) {
  if (profile_->ShutdownStarted()) {
    return;
  }
  CHECK(!IsUnderMemoryPressure() ||
        reason == ContainerCreationReason::kUserTriggeredColdStart);
  backfill_scheduler_.Cancel();
  if (warmed_container_ && warmed_container_->ShouldReloadOnShow()) {
    metrics_->RecordWarmedContainerFate(Metrics::WarmedContainerFate::kCrashed);
    warmed_container_ = nullptr;
  }

  if (!warmed_container_) {
    warmed_container_ = CreateContainer();
    expiry_timer_.Start(
        FROM_HERE, expiry_delay_,
        base::BindOnce(&GlicWebContentsWarmingPool::OnContainerExpired,
                       base::Unretained(this)));
    metrics_->OnWarmedContentCreated(reason);
  }
}

void GlicWebContentsWarmingPool::OnMemoryPressure(
    base::MemoryLimit memory_limit) {
  memory_limit_ = memory_limit;

  // Clear the warmed container when receiving critical memory pressure.
  // Pre-warming is suspended while the system remains under critical pressure.
  if (IsUnderMemoryPressure()) {
    Clear(ClearReason::kMemoryPressure);
    return;
  }

  // Refill the pool when memory pressure drops below critical, provided the
  // pool should maintain a container and doesn't already have one or a timer.
  if (should_warm_when_memory_allows_ && !warmed_container_ &&
      !backfill_scheduler_.IsScheduled()) {
    EnsurePreloadDelayed(ContainerCreationReason::kMemoryPressureRecovery);
  }
}

bool GlicWebContentsWarmingPool::IsUnderMemoryPressure() const {
  return memory_limit_ <= base::kCriticalMemoryPressureThreshold;
}

void GlicWebContentsWarmingPool::EnsurePreloadDelayed(
    ContainerCreationReason reason) {
  if (profile_->ShutdownStarted()) {
    return;
  }
  CHECK(!warmed_container_);
  if (IsUnderMemoryPressure()) {
    return;
  }
  if (backfill_scheduler_.IsScheduled()) {
    return;
  }
  backfill_scheduler_.Schedule(
      base::BindOnce(&GlicWebContentsWarmingPool::EnsurePreload,
                     base::Unretained(this), reason));
}

bool GlicWebContentsWarmingPool::HasWarmedContainerForTesting() const {
  return !!warmed_container_;
}

GlicWebContentsManager*
GlicWebContentsWarmingPool::GetWarmedContainerForTesting() const {
  return warmed_container_.get();
}

std::optional<GlicWebContentsWarmingPool::WarmedWebContents>
GlicWebContentsWarmingPool::GetWarmedWebContents() const {
  if (!warmed_container_) {
    return std::nullopt;
  }
  if (features::IsGlicNoWebviewEnabled()) {
    auto* no_webview_container =
        static_cast<GlicNoWebviewContentsManager*>(warmed_container_.get());
    return WarmedWebContents{
        .webui_contents = no_webview_container->overlay_contents(),
        .guest_contents = no_webview_container->guest_contents(),
    };
  }
  return WarmedWebContents{
      .webui_contents = warmed_container_->active_web_contents(),
      .guest_contents = warmed_container_->guest_contents(),
  };
}

}  // namespace glic
