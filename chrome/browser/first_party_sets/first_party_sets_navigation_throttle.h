// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_NAVIGATION_THROTTLE_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/timer/elapsed_timer.h"
#include "base/timer/timer.h"
#include "content/public/browser/navigation_throttle.h"

namespace content {
class NavigationHandle;
}  // namespace content

namespace first_party_sets {

class FirstPartySetsPolicyService;

// Observes navigations and defers navigations of outermost frames on
// First-Party Sets initialization during startup.
class FirstPartySetsNavigationThrottle : public content::NavigationThrottle {
 public:
  explicit FirstPartySetsNavigationThrottle(
      content::NavigationHandle* handle,
      FirstPartySetsPolicyService& service);
  ~FirstPartySetsNavigationThrottle() override;

  // content::NavigationThrottle:
  ThrottleCheckResult WillStartRequest() override;
  const char* GetNameForLogging() override;
  void Resume() override;

  // Only create throttle for the regular profile if FPS initialization has not
  // completed and FPS clearing is enabled and this is the outermost frame
  // navigation; returns nullptr otherwise.
  static std::unique_ptr<FirstPartySetsNavigationThrottle>
  MaybeCreateNavigationThrottle(content::NavigationHandle* navigation_handle);

  base::OneShotTimer& GetTimerForTesting() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return resume_navigation_timer_;
  }

 private:
  // Resume the navigation.
  void OnTimeOut();

  // Stop `resume_navigation_timer_` and resume the navigation.
  void OnReadyToResume();

  raw_ref<FirstPartySetsPolicyService> service_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // The timer used to resume the navigation on timeout.
  base::OneShotTimer resume_navigation_timer_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Timer starting when a navigation gets deferred.
  std::optional<base::ElapsedTimer> throttle_navigation_timer_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Stores the state of whether the navigation has been resumed, to make sure
  // the navigation is only resumed once.
  bool resumed_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<FirstPartySetsNavigationThrottle> weak_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};
};

}  // namespace first_party_sets

#endif  // CHROME_BROWSER_FIRST_PARTY_SETS_FIRST_PARTY_SETS_NAVIGATION_THROTTLE_H_
