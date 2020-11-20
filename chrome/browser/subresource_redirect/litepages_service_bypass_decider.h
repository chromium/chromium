// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUBRESOURCE_REDIRECT_LITEPAGES_SERVICE_BYPASS_DECIDER_H_
#define CHROME_BROWSER_SUBRESOURCE_REDIRECT_LITEPAGES_SERVICE_BYPASS_DECIDER_H_

#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"

// Interface to decide whether LitePages service should be bypassed, which is
// used for fetching compressed image and fetching robots.txt. Whenever an
// LitePages server fetch fails, subsequent fetches to LitePages is turned off
// for a random 1-5 minute duration or until the time mentioned in Retry-After
// response header from the server or up to a maximum duration specified by
// experimental params.
class LitePagesServiceBypassDecider
    : public base::SupportsWeakPtr<LitePagesServiceBypassDecider> {
 public:
  LitePagesServiceBypassDecider();
  ~LitePagesServiceBypassDecider();

  // Returns whether a fetch to LitePages service should be allowed.
  bool ShouldAllowNow();

  // Notifies the decider that a LitePages fetch had failed, with the
  // |response_code. This will start bypassing subsequent LitePage fetches.
  void NotifyFetchFailureWithResponseCode(int response_code,
                                          base::TimeDelta retry_after);

  // Notifies the decider that a LitePages fetch had failed, which will
  // start bypassing subsequent LitePage fetches.
  void NotifyFetchFailure(base::TimeDelta retry_after);

  base::Optional<base::TimeTicks> GetBypassUntilTimeForTesting() const {
    return bypassed_until_time_;
  }
  void SetBypassUntilTimeForTesting(base::TimeTicks bypass_until) {
    bypassed_until_time_ = bypass_until;
  }

 private:
  // The time until which image compression should be bypassed. Null time
  // indicates no bypass.
  base::Optional<base::TimeTicks> bypassed_until_time_;

  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // CHROME_BROWSER_SUBRESOURCE_REDIRECT_LITEPAGES_SERVICE_BYPASS_DECIDER_H_
