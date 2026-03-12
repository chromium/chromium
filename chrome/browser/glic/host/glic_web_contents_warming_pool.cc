// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_web_contents_warming_pool.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/time/time.h"
#include "chrome/browser/glic/host/webui_contents_container.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/web_contents.h"

namespace glic {

constexpr base::TimeDelta kDelayTooLong = base::Days(7);

GlicWebContentsWarmingPool::GlicWebContentsWarmingPool(Profile* profile)
    : profile_(profile) {}

GlicWebContentsWarmingPool::~GlicWebContentsWarmingPool() = default;

std::unique_ptr<WebUIContentsContainer>
GlicWebContentsWarmingPool::TakeContainer() {
  EnsurePreload();
  auto result = std::move(warmed_container_);
  EnsurePreloadDelayed();
  return result;
}

void GlicWebContentsWarmingPool::EnsurePreload() {
  CHECK(base::FeatureList::IsEnabled(features::kGlicWebContentsWarming));
  if (warmed_container_ && warmed_container_->web_contents()->IsCrashed()) {
    warmed_container_.reset();
  }
  if (warmed_container_) {
    return;
  }
  warmed_container_ = std::make_unique<WebUIContentsContainer>(
      profile_, /*initially_hidden=*/false);
}

void GlicWebContentsWarmingPool::Clear() {
  warmed_container_.reset();
  delay_timer_.Stop();
}

void GlicWebContentsWarmingPool::EnsurePreloadDelayed() {
  if (warmed_container_ && !warmed_container_->web_contents()->IsCrashed()) {
    return;
  }
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
  return warmed_container_ != nullptr;
}

}  // namespace glic
