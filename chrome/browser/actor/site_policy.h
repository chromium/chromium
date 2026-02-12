// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_SITE_POLICY_H_
#define CHROME_BROWSER_ACTOR_SITE_POLICY_H_

#include "base/functional/callback_forward.h"
#include "base/functional/function_ref.h"
#include "base/types/expected.h"
#include "chrome/browser/actor/enterprise_policy_url_checker.h"
#include "chrome/common/actor.mojom-forward.h"
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
class OriginChecker;

// Called during initialization of the given profile, to load the blocklist.
void InitActionBlocklist(Profile* profile);

enum class MayActOnUrlBlockReason {
  kAllowed,
  kExternalProtocol,
  kIpAddress,
  kLookalikeDomain,
  kOptimizationGuideBlock,
  kSafeBrowsing,
  kTabIsErrorDocument,
  kUrlNotInAllowlist,
  kWrongScheme,
  kEnterprisePolicy,
  kBlockedByStaticList,
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
// `policy_checker` is used to evaluate the URL based on enterprise policy
// allow/blocklists.
void MayActOnTab(const tabs::TabInterface& tab,
                 AggregatedJournal& journal,
                 TaskId task_id,
                 const OriginChecker& origin_checker,
                 const EnterprisePolicyUrlChecker& policy_checker,
                 DecisionCallbackWithReason callback);

// Like MayActOnTab, but considers a URL on its own.
// This can optionally allow insecure HTTP URLs as in practice sites may have
// HTTP links that will get upgraded. Rejecting HTTP URLs before this can happen
// would be too serious of an impediment.
void MayActOnUrl(const GURL& url,
                 bool allow_insecure_http,
                 Profile* profile,
                 AggregatedJournal& journal,
                 TaskId task_id,
                 const EnterprisePolicyUrlChecker& policy_checker,
                 DecisionCallbackWithReason callback);

// Checks if navigation to `url` should be blocked using
// OptimizationGuideService. If the callback is invoked with `may_act` set to
// `true`, then the actor is allowed to navigate to the URL. If `callback` is
// invoked with `false`, the actor should block navigation or ask the user to
// confirm.
//
// Returns `base::ok()` if this function will eventually invoke `callback`;
// otherwise returns `base::unexpected(callback)` and the caller is responsible
// for invoking `callback` themselves (maybe because the feature was disabled,
// or OptimizationGuide is not available for some other reason).
base::expected<void, DecisionCallback>
MaybeCheckOptimizationGuideForSensitiveUrl(const GURL& url,
                                           Profile* profile,
                                           DecisionCallback callback);

// Expresses the block reason as an action result.
// A code may be used for multiple cases if they don't need to be distinguished
// to the client.
// `for_navigation` indicates if a navigation is being evaluated. This should be
// false when checking the current page or validating tool parameters.
mojom::ActionResultCode BlockReasonToResultCode(MayActOnUrlBlockReason reason,
                                                bool for_navigation);

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_SITE_POLICY_H_
