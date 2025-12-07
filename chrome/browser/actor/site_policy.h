// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_SITE_POLICY_H_
#define CHROME_BROWSER_ACTOR_SITE_POLICY_H_

#include "base/functional/callback_forward.h"
#include "chrome/common/actor/task_id.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "url/origin.h"

namespace tabs {
class TabInterface;
}

class GURL;
class Profile;

namespace actor {

class AggregatedJournal;

// Called during initialization of the given profile, to load the blocklist.
void InitActionBlocklist(Profile* profile);

enum class MayActOnUrlBlockReason {
  kAllowed,
  kActuactionDisabled,
  kExternalProtocol,
  kIpAddress,
  kLookalikeDomain,
  kOptimizationGuideBlock,
  kSafeBrowsing,
  kTabIsErrorDocument,
  kUrlNotInAllowlist,
  kWrongScheme,
};

using DecisionCallback = base::OnceCallback<void(/*may_act=*/bool)>;
using DecisionCallbackWithReason =
    base::OnceCallback<void(MayActOnUrlBlockReason reason)>;

// Checks whether the actor may perform actions on the given tab based on the
// last committed document and URL. Invokes the callback with true if it is
// allowed.
// `MayActOnTab` takes a set of `allowed_origins` where for which do not apply
// the optimization guide check. We do so because `MayActOnTab` is called before
// any navigations can take place, so we need to check if the current URL when a
// task starts. However, any future URLs the actor navigates to should undergo
// blocklist checks in `MayActOnUrl` or
// `ShouldBlockNavigationUrlForOriginGating`.
// Please use ActorPolicyChecker instead of calling this directly.
void MayActOnTab(const tabs::TabInterface& tab,
                 AggregatedJournal& journal,
                 TaskId task_id,
                 const absl::flat_hash_set<url::Origin>& allowed_origins,
                 DecisionCallbackWithReason callback);

// Like MayActOnTab, but considers a URL on its own.
// This can optionally allow insecure HTTP URLs as in practice sites may have
// HTTP links that will get upgraded. Rejecting HTTP URLs before this can happen
// would be too serious of an impediment.
// Please use ActorPolicyChecker instead of calling this directly.
void MayActOnUrl(const GURL& url,
                 bool allow_insecure_http,
                 Profile* profile,
                 AggregatedJournal& journal,
                 TaskId task_id,
                 DecisionCallback callback);

// Same as above, but the callback includes a `MayActOnUrlBlockReason`.
// TODO(crbug.com/458045204): Migrate callers of other function to use this one
// instead.
void MayActOnUrl(const GURL& url,
                 bool allow_insecure_http,
                 Profile* profile,
                 AggregatedJournal& journal,
                 TaskId task_id,
                 DecisionCallbackWithReason callback);

// Checks if navigation to `url` should be blocked using
// OptimizationGuideService. If the callback is invoked with `may_act` set to
// `true`, then the actor is allowed to navigate to the URL. Otherwise, the
// actor should block navigation or ask the user to confirm.
bool ShouldBlockNavigationUrlForOriginGating(const GURL& url,
                                             Profile* profile,
                                             DecisionCallback callback);

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_SITE_POLICY_H_
