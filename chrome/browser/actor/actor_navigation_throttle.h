// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ACTOR_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_ACTOR_ACTOR_NAVIGATION_THROTTLE_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/types/pass_key.h"
#include "chrome/browser/actor/aggregated_journal.h"
#include "chrome/browser/actor/site_policy.h"
#include "chrome/common/actor/task_id.h"
#include "chrome/common/actor_webui.mojom-forward.h"
#include "content/public/browser/navigation_throttle.h"
#include "url/gurl.h"

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
  class Delegate {
   public:
    // Callback invoked to confirm or cancel the deferred navigation.
    // If the argument is `true`, the user accepted to navigate away, and the
    // navigation will continue. The task will be canceled. If `false`,
    // the navigation is canceled, and the task will continue running.
    using NavigationConfirmedCallback = base::OnceCallback<void(bool)>;

    virtual ~Delegate() = default;

    // Called when a user initiated navigation is detected while a tab is under
    // actor control to show a confirmation UI. If `MaybeDeferNavigation`
    // returns `true` then the navigation to url will be deferred until
    // `callback` is invoked.
    virtual bool MaybeDeferNavigation(const GURL& url,
                                      NavigationConfirmedCallback callback) = 0;
  };

  static ActorNavigationThrottle CreateForTesting(
      content::NavigationThrottleRegistry& registry,
      const ActorTask& task);
  ActorNavigationThrottle(base::PassKey<ActorNavigationThrottle>,
                          content::NavigationThrottleRegistry& registry,
                          const ActorTask& task);
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

  content::NavigationThrottle::ThrottleCheckResult WillStartOrRedirectRequest(
      bool is_redirection);

  void OnMayActOnUrlResult(
      std::unique_ptr<AggregatedJournal::PendingAsyncEntry> journal_entry,
      MayActOnUrlBlockReason block_reason);

  // Adds to the journal and resumes/cancels the navigation if needed. Must not
  // be called for prerendered main frame navigations.
  void OnNavigationConfirmationDecision(bool was_deferred, bool may_continue);

  // Decision handlers for navigation throttle outcomes.
  void OnUserLeaveDialogDecision(bool may_continue);

  Profile* GetProfile();
  AggregatedJournal& GetJournal();

  TaskId task_id_;
  base::WeakPtr<ExecutionEngine> execution_engine_;

  base::WeakPtrFactory<ActorNavigationThrottle> weak_factory_{this};
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_ACTOR_NAVIGATION_THROTTLE_H_
