// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_web_contents_warming_pool.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chrome/browser/glic/host/webui_contents_container.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/web_contents.h"

namespace glic {
namespace {
constexpr base::TimeDelta kDelayTooLong = base::Days(7);
}

class GlicWebContentsWarmingPool::Metrics {
 public:
  void OnContainerExpired() { was_expired_ = true; }

  void OnWarmedContentCreated() {
    warmed_container_creation_time_ = base::TimeTicks::Now();
    was_expired_ = false;
  }

  GlicWebContentsWarmingPool::WarmingPoolStatus RecordTakeContainerStatus(
      const std::unique_ptr<WebUIContentsContainer>& warmed_container) {
    WarmingPoolStatus status = WarmingPoolStatus::kCold;
    if (warmed_container) {
      status = warmed_container->web_contents()->IsCrashed()
                   ? WarmingPoolStatus::kCrashed
                   : WarmingPoolStatus::kHit;
      warmed_container_creation_time_ = warmed_container->creation_time();
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
    : profile_(profile), metrics_(std::make_unique<Metrics>()) {}

GlicWebContentsWarmingPool::~GlicWebContentsWarmingPool() = default;

std::unique_ptr<WebUIContentsContainer>
GlicWebContentsWarmingPool::TakeContainer() {
  if (warmed_container_) {
    expiry_timer_.Stop();
  }
  metrics_->RecordTakeContainerStatus(warmed_container_);

  EnsurePreload();
  std::unique_ptr<WebUIContentsContainer> result = std::move(warmed_container_);
  warmed_container_ = nullptr;
  EnsurePreloadDelayed();
  return result;
}

void GlicWebContentsWarmingPool::EnsurePreload() {
  CHECK(base::FeatureList::IsEnabled(features::kGlicWebContentsWarming));
  if (warmed_container_ && warmed_container_->web_contents()->IsCrashed()) {
    warmed_container_ = nullptr;
  }

  if (!warmed_container_) {
    warmed_container_ = CreateContainer();
    expiry_timer_.Start(
        FROM_HERE, features::kGlicWebContentsWarmingPoolExpiryDelay.Get(),
        base::BindOnce(&GlicWebContentsWarmingPool::OnContainerExpired,
                       base::Unretained(this)));
    metrics_->OnWarmedContentCreated();
  }
}

std::unique_ptr<WebUIContentsContainer>
GlicWebContentsWarmingPool::CreateContainer() {
  return std::make_unique<WebUIContentsContainerImpl>(
      profile_, /*initially_hidden=*/false);
}

void GlicWebContentsWarmingPool::OnContainerExpired() {
  if (warmed_container_) {
    metrics_->OnContainerExpired();
    Clear();
  }
}

void GlicWebContentsWarmingPool::Clear() {
  warmed_container_.reset();
  delay_timer_.Stop();
  expiry_timer_.Stop();
}

void GlicWebContentsWarmingPool::EnsurePreloadDelayed() {
  CHECK(!warmed_container_);
  if (delay_timer_.IsRunning()) {
    return;
  }
  auto delay = features::kGlicWebContentsWarmingDelay.Get();
  if (delay >= kDelayTooLong) {
    return;
  }
  delay_timer_.Start(FROM_HERE, delay,
                     base::BindOnce(&GlicWebContentsWarmingPool::EnsurePreload,
                                    base::Unretained(this)));
}

bool GlicWebContentsWarmingPool::HasWarmedContainerForTesting() const {
  return !!warmed_container_;
}

WebUIContentsContainer*
GlicWebContentsWarmingPool::GetWarmedContainerForTesting() const {
  return warmed_container_.get();
}

}  // namespace glic
