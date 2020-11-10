// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefetch/prefetch_proxy/prefetch_proxy_origin_decider.h"

#include "base/time/default_clock.h"
#include "chrome/browser/prefetch/prefetch_proxy/prefetch_proxy_params.h"
#include "chrome/browser/profiles/profile.h"

PrefetchProxyOriginDecider::PrefetchProxyOriginDecider()
    : clock_(base::DefaultClock::GetInstance()) {}

PrefetchProxyOriginDecider::~PrefetchProxyOriginDecider() = default;

void PrefetchProxyOriginDecider::SetClockForTesting(const base::Clock* clock) {
  clock_ = clock;
}

void PrefetchProxyOriginDecider::OnBrowsingDataCleared() {
  origin_retry_afters_.clear();
}

bool PrefetchProxyOriginDecider::IsOriginOutsideRetryAfterWindow(
    const GURL& url) const {
  url::Origin origin = url::Origin::Create(url);

  auto iter = origin_retry_afters_.find(origin);
  if (iter == origin_retry_afters_.end()) {
    return true;
  }

  return iter->second < clock_->Now();
}

void PrefetchProxyOriginDecider::ReportOriginRetryAfter(
    const GURL& url,
    base::TimeDelta retry_after) {
  // Ignore negative times.
  if (retry_after < base::TimeDelta()) {
    return;
  }

  // Cap values at a maximum per experiment.
  if (retry_after > PrefetchProxyMaxRetryAfterDelta()) {
    retry_after = PrefetchProxyMaxRetryAfterDelta();
  }

  origin_retry_afters_.emplace(url::Origin::Create(url),
                               clock_->Now() + retry_after);
}
