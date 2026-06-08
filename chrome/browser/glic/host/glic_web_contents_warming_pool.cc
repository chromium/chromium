// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_web_contents_warming_pool.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/glic/host/webui_contents_container.h"
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
constexpr base::TimeDelta kDelayTooLong = base::Days(7);
}

class GlicWebContentsWarmingPool::Metrics {
 public:
  // LINT.IfChange(GlicWarmedContainerFate)
  enum class WarmedContainerFate {
    kUsed = 0,
    kExpired = 1,
    kDeletedOnChromeClosed = 2,
    kCrashed = 3,
    kDeletedOnMemoryPressure = 4,
    kMaxValue = kDeletedOnMemoryPressure,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicWarmedContainerFate)

  void OnContainerExpired() {
    was_expired_ = true;
    RecordWarmedContainerFate(WarmedContainerFate::kExpired);
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
    was_expired_ = false;
  }

  void RecordWarmedContainerFate(WarmedContainerFate fate) {
    base::UmaHistogramEnumeration("Glic.WarmingPool.WarmedContainerFate", fate);
  }

  GlicWebContentsWarmingPool::WarmingPoolStatus RecordTakeContainerStatus(
      const std::unique_ptr<WebUIContentsContainer>& warmed_container) {
    WarmingPoolStatus status = WarmingPoolStatus::kCold;
    if (warmed_container) {
      status = warmed_container->web_contents()->IsCrashed()
                   ? WarmingPoolStatus::kCrashed
                   : WarmingPoolStatus::kHit;
      warmed_container_creation_time_ = warmed_container->creation_time();
      if (status == WarmingPoolStatus::kHit) {
        RecordWarmedContainerFate(WarmedContainerFate::kUsed);
      }
    } else if (was_expired_) {
      status = WarmingPoolStatus::kExpired;
    }

    base::UmaHistogramEnumeration("Glic.WarmingPool.HitStatus", status);
    RecordTimeSinceCreatedAt(status);

    if (status != WarmingPoolStatus::kHit) {
      was_expired_ = false;
    }
    return status;
  }

  void RecordClearWarmedContainer(
      const std::unique_ptr<WebUIContentsContainer>& warmed_container,
      std::optional<ClearReason> reason) {
    if (!warmed_container || !reason) {
      return;
    }
    WarmedContainerFate fate;
    switch (*reason) {
      case ClearReason::kShutdown:
        fate = WarmedContainerFate::kDeletedOnChromeClosed;
        break;
      case ClearReason::kMemoryPressure:
        fate = WarmedContainerFate::kDeletedOnMemoryPressure;
        break;
    }
    RecordWarmedContainerFate(fate);
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

  // Whether the warmed_container_ was missing because of the expiry timer.
  bool was_expired_ = false;

  // Creation time of the warmed_container_. For misses, this is preserved
  // from the most recently destroyed container until reported.
  std::optional<base::TimeTicks> warmed_container_creation_time_;
};

GlicWebContentsWarmingPool::GlicWebContentsWarmingPool(Profile* profile)
    : profile_(profile), metrics_(std::make_unique<Metrics>()) {
  if (base::FeatureList::IsEnabled(features::kGlicWebContentsWarming)) {
    expiry_delay_ = features::kGlicWebContentsWarmingPoolExpiryDelay.Get();
    warming_delay_ = features::kGlicWebContentsWarmingDelay.Get();
  }
}

GlicWebContentsWarmingPool::~GlicWebContentsWarmingPool() {
  Clear(ClearReason::kShutdown);
}

std::unique_ptr<WebUIContentsContainer>
GlicWebContentsWarmingPool::TakeContainer() {
  metrics_->RecordTakeContainerStatus(warmed_container_);
  reload_count_ = 0;

  EnsurePreload(ContainerCreationReason::kUserTriggeredColdStart);
  std::unique_ptr<WebUIContentsContainer> result = std::move(warmed_container_);
  warmed_container_ = nullptr;
  expiry_timer_.Stop();

  EnsurePreloadDelayed(ContainerCreationReason::kRefill);
  return result;
}

void GlicWebContentsWarmingPool::EnsurePreload(ContainerCreationReason reason) {
  if (warmed_container_ && warmed_container_->web_contents()->IsCrashed()) {
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

std::unique_ptr<WebUIContentsContainer>
GlicWebContentsWarmingPool::CreateContainer() {
  TRACE_EVENT("glic", "GlicWebContentsWarmingPool::CreateContainer");
  bool initially_hidden =
      base::FeatureList::IsEnabled(features::kGlicContentsInitiallyHidden);
  return std::make_unique<WebUIContentsContainerImpl>(profile_,
                                                      initially_hidden);
}

void GlicWebContentsWarmingPool::OnContainerExpired() {
  if (warmed_container_) {
    TRACE_EVENT_INSTANT("glic",
                        "GlicWebContentsWarmingPool::OnContainerExpired");
    metrics_->OnContainerExpired();
    Clear(std::nullopt);
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
        metrics_->OnReloadAfterExpiry(
            GlicWebContentsWarmingPool::ReloadAfterExpiryStatus::
                kNotReloadedLimitReached);
      }
    } else {
      metrics_->OnReloadAfterExpiry(
          GlicWebContentsWarmingPool::ReloadAfterExpiryStatus::
              kNotReloadedFeatureDisabled);
    }
  }
}

void GlicWebContentsWarmingPool::Clear(std::optional<ClearReason> reason) {
  metrics_->RecordClearWarmedContainer(warmed_container_, reason);
  warmed_container_.reset();
  delay_timer_.Stop();
  expiry_timer_.Stop();
}

void GlicWebContentsWarmingPool::EnsurePreloadDelayed(
    ContainerCreationReason reason) {
  CHECK(!warmed_container_);
  if (delay_timer_.IsRunning()) {
    return;
  }
  auto delay = warming_delay_;
  if (delay >= kDelayTooLong) {
    return;
  }
  delay_timer_.Start(FROM_HERE, delay,
                     base::BindOnce(&GlicWebContentsWarmingPool::EnsurePreload,
                                    base::Unretained(this), reason));
}

bool GlicWebContentsWarmingPool::HasWarmedContainerForTesting() const {
  return !!warmed_container_;
}

WebUIContentsContainer*
GlicWebContentsWarmingPool::GetWarmedContainerForTesting() const {
  return warmed_container_.get();
}

content::WebContents* GlicWebContentsWarmingPool::GetWarmedWebContents() const {
  return warmed_container_ ? warmed_container_->web_contents() : nullptr;
}

}  // namespace glic
