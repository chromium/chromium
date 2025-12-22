// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_SITE_POLICY_H_
#define CHROME_BROWSER_ACTOR_SITE_POLICY_H_

#include "base/functional/callback_forward.h"
#include "base/types/strong_alias.h"
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
  kEnterprisePolicy,
};

using DecisionCallback = base::OnceCallback<void(/*may_act=*/bool)>;
using DecisionCallbackWithReason =
    base::OnceCallback<void(MayActOnUrlBlockReason reason)>;

// Types for sets of origins that the actor is able to navigate to.
// The first, AllowedOriginSet, is the set of origins that have been approved
// for the actor by the server. The second, ConfirmedOriginSet, is the set of
// sensitive origins that the user has manually confirmed the actor may interact
// with. The use of StrongAlias is to convey that these two sets should not be
// used interchangeably and to enforce this at compile time.
using AllowedOriginSet = base::StrongAlias<class AllowedOriginSetTag,
                                           absl::flat_hash_set<url::Origin>>;
using ConfirmedOriginSet = base::StrongAlias<class ConfirmedOriginSetTag,
                                             absl::flat_hash_set<url::Origin>>;

enum class EnterprisePolicyBlockReason {
  // Enterprise policy did not explicitly block a URL, but it also did not
  // explicitly allow it, so the regular safety checks should still be done.
  kNotBlocked,
  // Enterprise policy explicitly allowed a URL. Some additional safety checks
  // are not done.
  kExplicitlyAllowed,
  // Enterprise policy explicitly blocked a URL.
  kExplicitlyBlocked,
};

using EnterprisePolicyCallback =
    base::OnceCallback<EnterprisePolicyBlockReason(const GURL&)>;

// Checks whether the actor may perform actions on the given tab based on the
// last committed document and URL. Invokes the callback with true if it is
// allowed.
// `MayActOnTab` takes a set of `allowed_origins` where for which do not apply
// the optimization guide check. We do so because `MayActOnTab` is called before
// any navigations can take place, so we need to check if the current URL when a
// task starts. However, any future URLs the actor navigates to should undergo
// blocklist checks in `MayActOnUrl` or
// `ShouldBlockNavigationUrlForOriginGating`.
// `enterprise_policy_eval_url` returns the evaluation of the URL based on
// enterprise policy allow/blocklists.
// Please use ActorPolicyChecker instead of calling this directly.
void MayActOnTab(const tabs::TabInterface& tab,
                 AggregatedJournal& journal,
                 TaskId task_id,
                 const ConfirmedOriginSet& confirmed_origins,
                 EnterprisePolicyCallback enterprise_policy_eval_url,
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
                 EnterprisePolicyCallback enterprise_policy_eval_url,
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
