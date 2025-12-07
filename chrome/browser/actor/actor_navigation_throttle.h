// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ACTOR_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_ACTOR_ACTOR_NAVIGATION_THROTTLE_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/actor/aggregated_journal.h"
#include "chrome/browser/actor/site_policy.h"
#include "chrome/common/actor/task_id.h"
#include "chrome/common/actor_webui.mojom-forward.h"
#include "content/public/browser/navigation_throttle.h"

namespace content {
class NavigationThrottleRegistry;
}

class Profile;

namespace actor {

class ActorTask;
class ExecutionEngine;

// Throttles navigations in tabs under the control of the actor in order to
// apply safety policies.
// For example, if the actor clicks a link to a blocked site, that'll be
// intercepted here.
class ActorNavigationThrottle : public content::NavigationThrottle {
 public:
  static void MaybeCreateAndAdd(content::NavigationThrottleRegistry& registry);

  ActorNavigationThrottle(const ActorNavigationThrottle&) = delete;
  ActorNavigationThrottle& operator=(const ActorNavigationThrottle&) = delete;

  ~ActorNavigationThrottle() override;

  // content::NavigationThrottle:
  content::NavigationThrottle::ThrottleCheckResult WillStartRequest() override;
  content::NavigationThrottle::ThrottleCheckResult WillRedirectRequest()
      override;
  content::NavigationThrottle::ThrottleCheckResult WillProcessResponse()
      override;
  const char* GetNameForLogging() override;

 private:
  explicit ActorNavigationThrottle(
      content::NavigationThrottleRegistry& registry,
      const ActorTask& task);

  content::NavigationThrottle::ThrottleCheckResult WillStartOrRedirectRequest(
      bool is_redirection);

  void OnMayActOnUrlResult(
      std::unique_ptr<AggregatedJournal::PendingAsyncEntry> journal_entry,
      MayActOnUrlBlockReason block_reason);

  void OnNavigationConfirmationDecision(bool may_continue);

  Profile* GetProfile();
  AggregatedJournal& GetJournal();

  TaskId task_id_;
  base::WeakPtr<ExecutionEngine> execution_engine_;

  base::WeakPtrFactory<ActorNavigationThrottle> weak_factory_{this};
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_ACTOR_NAVIGATION_THROTTLE_H_
