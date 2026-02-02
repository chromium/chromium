// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ENTERPRISE_POLICY_CHECKER_H_
#define CHROME_BROWSER_ACTOR_ENTERPRISE_POLICY_CHECKER_H_

class GURL;

namespace actor {

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

// Interface for actor clients to provide policy checks for actor usage
// generally as well as on a per URL basis.
class EnterprisePolicyChecker {
 public:
  enum class CannotActReason {
    // Browser can actuate.
    kNone,
    // The enterprise policy disables the actuation feature. Only applicable to
    // managed clients (Profile level, browser level or machine level).
    kDisabledByPolicy,
    // The account is not eligible for the actuation.
    kAccountCapabilityIneligible,
    // The account is not subscribed to one of the required AI subscription
    // tiers.
    kAccountMissingChromeBenefits,
    // An enterprise account is logged in but there is no management to deliver
    // the policy. Actuation is disabled because the policy pref default value
    // is disabled.
    kEnterpriseWithoutManagement,
  };

  // Whether or not the client has policy permissions to use Actor. This can
  // change at runtime but, if necessary, clients are expected to stop any
  // active tasks if permission is revoked after creating a task.
  virtual bool CanActOnWeb() const = 0;

  // The reason why `CanActOnWeb()` returns false (or `kNone` otherwise).
  // The `CanActOnWeb()` method should be used for feature logic; this method
  // is intended for presenting additional information (to the user,  or for
  // debugging) where useful.
  virtual CannotActReason CannotActOnWebReason() const = 0;

  // Returns whether or not the actor is allowed to act on the provided URL.
  virtual EnterprisePolicyBlockReason Evaluate(const GURL& url) const = 0;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_ENTERPRISE_POLICY_CHECKER_H_
