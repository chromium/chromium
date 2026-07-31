// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_ACTOR_LOGIN_FLOW_VERIFIER_H_
#define CHROME_BROWSER_ACTOR_TOOLS_ACTOR_LOGIN_FLOW_VERIFIER_H_

#include <memory>
#include <optional>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/autofill/actor/one_time_tokens/actor_login_context.h"
#include "components/affiliations/core/browser/domain_matching/domain_relation_checker.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "url/origin.h"

namespace affiliations {
class AffiliationService;
}  // namespace affiliations

namespace actor {

// Verifies whether an OTP filling attempt corresponds to a valid actor login
// flow.
class ActorLoginFlowVerifier {
 public:
  explicit ActorLoginFlowVerifier(
      affiliations::AffiliationService& affiliation_service);

  ActorLoginFlowVerifier(const ActorLoginFlowVerifier&) = delete;
  ActorLoginFlowVerifier& operator=(const ActorLoginFlowVerifier&) = delete;

  virtual ~ActorLoginFlowVerifier();

  // Checks if the tool execution corresponds to an actor login's sign in flow.
  // This is used to determine if we can skip the confirmation UI.
  //
  // The verification consists of the following checks:
  // 1. Verify the target OTP frame was tracked during the login attempt
  //    (checked via `navigations_per_frame` in `ActorLoginContext`).
  // 2. Verify that no tracked login frames have navigated more than once
  //    (multiple redirects likely exit the sign-in flow).
  // 3. Verify the OTP frame origin is related to the main frame origin of the
  //    login attempt (via `DomainRelationChecker`).
  // 4. Verify the match strength complies with `should_use_strong_matching`
  //    (rejecting grouped affiliations, and allowing PSL only for weak
  //    matching).
  virtual void VerifyIsActorLoginFlow(
      content::FrameTreeNodeId otp_frame_id,
      const url::Origin& otp_frame_origin,
      const url::Origin& main_frame_origin,
      const std::optional<autofill::ActorLoginContext>& context,
      base::OnceCallback<void(bool)> callback);

 private:
  void OnMainFrameOriginMatchEvaluated(
      const url::Origin& otp_frame_origin,
      const url::Origin& context_origin,
      bool should_use_strong_matching,
      base::OnceCallback<void(bool)> callback,
      std::optional<affiliations::MatchType> match_type);

  affiliations::DomainRelationChecker domain_relation_checker_;
  base::WeakPtrFactory<ActorLoginFlowVerifier> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_ACTOR_LOGIN_FLOW_VERIFIER_H_
