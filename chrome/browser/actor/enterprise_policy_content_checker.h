// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ENTERPRISE_POLICY_CONTENT_CHECKER_H_
#define CHROME_BROWSER_ACTOR_ENTERPRISE_POLICY_CONTENT_CHECKER_H_

#include "base/functional/callback.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace actor {

// Interface for actor clients to provide policy checks for actor usage on
// content being sent to a renderer.
class EnterprisePolicyContentChecker {
 public:
  enum class ValidationReason {
    // Enterprise policy found the content compliant.
    kAllowed,
    // Enterprise policy found issues with the content but allowed the user to
    // proceed with a warning.
    kWarned,
    // Enterprise policy blocked the content.
    kBlocked,
  };
  using ValidationCallback = base::OnceCallback<void(ValidationReason)>;

  // Validates async whether the provided content is allowed to be injected into
  // the renderer by enterprise policy.
  virtual void ValidateContentSentToRenderer(content::RenderFrameHost* frame,
                                             const std::string& content,
                                             ValidationCallback callback) = 0;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_ENTERPRISE_POLICY_CONTENT_CHECKER_H_
