// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/subresource_redirect/litepages_service_bypass_decider.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/subresource_redirect/subresource_redirect_util.h"
#include "components/subresource_redirect/common/subresource_redirect_features.h"
#include "net/http/http_status_code.h"
#include "third_party/blink/public/common/features.h"

LitePagesServiceBypassDecider::LitePagesServiceBypassDecider() = default;

LitePagesServiceBypassDecider::~LitePagesServiceBypassDecider() = default;

bool LitePagesServiceBypassDecider::ShouldAllowNow() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(subresource_redirect::ShouldEnablePublicImageHintsBasedCompression() ||
         subresource_redirect::ShouldEnableRobotsRulesFetching());
  bool should_allow =
      !bypassed_until_time_ || base::TimeTicks::Now() > bypassed_until_time_;
  base::UmaHistogramBoolean("SubresourceRedirect.LitePagesService.BypassResult",
                            !should_allow);
  return should_allow;
}

void LitePagesServiceBypassDecider::NotifyFetchFailureWithResponseCode(
    int response_code,
    base::TimeDelta retry_after) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(subresource_redirect::ShouldEnablePublicImageHintsBasedCompression() ||
         subresource_redirect::ShouldEnableRobotsRulesFetching());
  if (response_code == net::HTTP_SERVICE_UNAVAILABLE ||
      response_code == net::HTTP_FORBIDDEN) {
    NotifyFetchFailure(retry_after);
  }
}

void LitePagesServiceBypassDecider::NotifyFetchFailure(
    base::TimeDelta retry_after) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(subresource_redirect::ShouldEnablePublicImageHintsBasedCompression() ||
         subresource_redirect::ShouldEnableRobotsRulesFetching());
  if (!retry_after.is_zero()) {
    // Choose the time mentioned in retry_after, but cap it to a max value.
    retry_after = std::min(
        retry_after, subresource_redirect::GetLitePagesBypassMaxDuration());
  } else {
    // Bypass for a random duration.
    retry_after = subresource_redirect::GetLitePagesBypassRandomDuration();
  }
  // Take the maximum possible bypass duration.
  bypassed_until_time_ = bypassed_until_time_
                             ? std::max(*bypassed_until_time_,
                                        base::TimeTicks::Now() + retry_after)
                             : base::TimeTicks::Now() + retry_after;
  base::UmaHistogramLongTimes("SubresourceRedirect.BypassDuration",
                              retry_after);
}
