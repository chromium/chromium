// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ACTOR_ENTERPRISE_POLICY_CHECKER_H_
#define CHROME_BROWSER_ACTOR_ACTOR_ENTERPRISE_POLICY_CHECKER_H_

#include <string>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/actor/enterprise_policy_checker.h"
#include "components/policy/core/browser/url_list/url_blocklist_manager.h"

class GURL;
class Profile;

namespace content {
class RenderFrameHost;
}  // namespace content

namespace actor {

// Evaluates enterprise URL allowlist/blocklist and content protection policies
// for actor tasks.
class ActorEnterprisePolicyChecker : public EnterprisePolicyChecker {
 public:
  explicit ActorEnterprisePolicyChecker(Profile* profile);
  ActorEnterprisePolicyChecker(const ActorEnterprisePolicyChecker&) = delete;
  ActorEnterprisePolicyChecker& operator=(const ActorEnterprisePolicyChecker&) =
      delete;
  ~ActorEnterprisePolicyChecker() override;

  // Registers `callback` to be notified whenever the enterprise URL allowlist
  // or blocklist policies are updated.
  base::CallbackListSubscription AddUrlListsUpdateObserver(
      base::RepeatingClosure callback);

  // EnterprisePolicyChecker:
  UrlBlockReason Evaluate(const GURL& url) const override;
  void ValidateContentSentToRenderer(
      content::RenderFrameHost* frame,
      const std::string& content,
      ContentValidationCallback callback) const override;

 private:
  raw_ptr<Profile> profile_;
  policy::URLBlocklistManager url_blocklist_manager_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_ACTOR_ENTERPRISE_POLICY_CHECKER_H_
