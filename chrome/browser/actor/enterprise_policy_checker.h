// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ENTERPRISE_POLICY_CHECKER_H_
#define CHROME_BROWSER_ACTOR_ENTERPRISE_POLICY_CHECKER_H_

#include "base/functional/callback.h"

class GURL;

namespace content {
class RenderFrameHost;
}  // namespace content

namespace actor {

// Interface for actor clients to provide policy checks for actor usage on a
// given URL and content being sent to a renderer.
class EnterprisePolicyChecker {
 public:
  enum class UrlBlockReason {
    // Enterprise policy did not explicitly block a URL, but it also did not
    // explicitly allow it, so the regular safety checks should still be done.
    kNotBlocked,
    // Enterprise policy explicitly allowed a URL. Some additional safety checks
    // are not done.
    kExplicitlyAllowed,
    // Enterprise policy explicitly blocked a URL.
    kExplicitlyBlocked,
  };

  enum class ContentValidationReason {
    // Enterprise policy found the content compliant.
    kAllowed,
    // Enterprise policy found issues with the content but allowed the user to
    // proceed with a warning.
    kWarned,
    // Enterprise policy blocked the content.
    kBlocked,
  };

  using ContentValidationCallback =
      base::OnceCallback<void(ContentValidationReason)>;

  virtual ~EnterprisePolicyChecker() = default;

  // Returns whether or not the actor is allowed to act on the provided URL.
  virtual UrlBlockReason Evaluate(const GURL& url) const = 0;

  // Validates async whether the provided content is allowed to be injected into
  // the renderer by enterprise policy.
  virtual void ValidateContentSentToRenderer(
      content::RenderFrameHost* frame,
      const std::string& content,
      ContentValidationCallback callback) const = 0;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_ENTERPRISE_POLICY_CHECKER_H_
