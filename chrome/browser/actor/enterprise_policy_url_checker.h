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

// Interface for actor clients to provide policy checks for actor usage on a
// given URL.
class EnterprisePolicyUrlChecker {
 public:
  // Returns whether or not the actor is allowed to act on the provided URL.
  virtual EnterprisePolicyBlockReason Evaluate(const GURL& url) const = 0;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_ENTERPRISE_POLICY_CHECKER_H_
