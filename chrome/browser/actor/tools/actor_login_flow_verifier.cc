// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/actor_login_flow_verifier.h"

#include <algorithm>
#include <ostream>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/autofill/actor/one_time_tokens/actor_login_context.h"
#include "components/affiliations/core/browser/domain_matching/domain_relation_checker.h"
#include "components/affiliations/core/browser/match_type.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "url/origin.h"

namespace actor {

namespace {

using Result = ActorLoginFlowVerifier::Result;

Result GetAffiliationMatchVerificationResult(
    affiliations::MatchType affiliations_match_type,
    bool should_use_strong_matching) {
  // Exact or affiliated matches are always allowed.
  bool is_exact_match =
      affiliations_match_type == affiliations::MatchType::kExact;

  bool is_affiliated_match =
      static_cast<int>(affiliations_match_type) &
      static_cast<int>(affiliations::MatchType::kAffiliated);

  bool is_psl_match = static_cast<int>(affiliations_match_type) &
                      static_cast<int>(affiliations::MatchType::kPSL);

  if (is_exact_match) {
    return Result::kExactMatchAllowed;
  }

  if (is_affiliated_match) {
    return Result::kAffiliatedMatchAllowed;
  }

  if (is_psl_match) {
    if (should_use_strong_matching) {
      return Result::kPslMatchDisallowed;
    }
    return Result::kPslMatchAllowed;
  }

  return Result::kGroupedOrOtherMismatch;
}

}  // namespace

ActorLoginFlowVerifier::ActorLoginFlowVerifier(
    affiliations::AffiliationService& affiliation_service)
    : domain_relation_checker_(affiliation_service) {}

ActorLoginFlowVerifier::~ActorLoginFlowVerifier() = default;

void ActorLoginFlowVerifier::VerifyIsActorLoginFlow(
    content::FrameTreeNodeId otp_frame_id,
    const url::Origin& otp_frame_origin,
    const url::Origin& main_frame_origin,
    std::optional<url::Origin> context_origin,
    bool should_use_strong_matching,
    base::OnceCallback<std::optional<autofill::ActorLoginContext>()>
        consume_context_callback,
    base::OnceCallback<void(Result)> callback) {
  if (!context_origin.has_value()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), Result::kNoActorLoginContext));
    return;
  }

  // Verify that the login flow started on the same origin (or an affiliated
  // one) as the current main frame origin. This prevents silent filling if the
  // user navigated away from the original login flow to a different, unrelated
  // website.
  domain_relation_checker_.Check(
      *context_origin, main_frame_origin,
      base::BindOnce(&ActorLoginFlowVerifier::OnMainFrameOriginMatchEvaluated,
                     weak_ptr_factory_.GetWeakPtr(), otp_frame_id,
                     otp_frame_origin, *context_origin,
                     should_use_strong_matching,
                     std::move(consume_context_callback), std::move(callback)));
}

void ActorLoginFlowVerifier::OnMainFrameOriginMatchEvaluated(
    content::FrameTreeNodeId otp_frame_id,
    const url::Origin& otp_frame_origin,
    const url::Origin& context_origin,
    bool should_use_strong_matching,
    base::OnceCallback<std::optional<autofill::ActorLoginContext>()>
        consume_context_callback,
    base::OnceCallback<void(Result)> callback,
    std::optional<affiliations::MatchType> match_type) {
  if (!match_type.has_value()) {
    std::move(callback).Run(Result::kMainFrameOriginMismatch);
    return;
  }

  // Only exact or affiliated matches are allowed for the main frame.
  bool is_exact_match = *match_type == affiliations::MatchType::kExact;
  bool is_affiliated_match =
      static_cast<int>(*match_type) &
      static_cast<int>(affiliations::MatchType::kAffiliated);
  if (!is_exact_match && !is_affiliated_match) {
    std::move(callback).Run(Result::kMainFrameOriginMismatch);
    return;
  }

  // Last check: verify OTP form origin and main frame origin are related.
  // We need to make sure that we don't skip user confirmation for OTPs that do
  // not belong to actor login flows. Actor login fills credentials in all
  // iframes that it considers trustworthy because it doesn't know which one
  // contains the correct login form. It also uses 2 different trust levels
  // (based on user permission type), both are based on iframe's and main
  // frame's origins. This method needs to match the same trust levels, hence
  // the `should_use_strong_matching` parameter. To avoid checking each filled
  // frame, we try to match the OTP form's origin with the origin of the main
  // frame where actor login flow started and rely on the fact that affiliations
  // are transitive.
  domain_relation_checker_.Check(
      context_origin, otp_frame_origin,
      base::BindOnce(&ActorLoginFlowVerifier::OnOtpFrameOriginMatchEvaluated,
                     weak_ptr_factory_.GetWeakPtr(), otp_frame_id,
                     should_use_strong_matching,
                     std::move(consume_context_callback), std::move(callback)));
}

void ActorLoginFlowVerifier::OnOtpFrameOriginMatchEvaluated(
    content::FrameTreeNodeId otp_frame_id,
    bool should_use_strong_matching,
    base::OnceCallback<std::optional<autofill::ActorLoginContext>()>
        consume_context_callback,
    base::OnceCallback<void(Result)> callback,
    std::optional<affiliations::MatchType> affiliations_match_type) {
  if (!affiliations_match_type.has_value()) {
    std::move(callback).Run(Result::kNoMatch);
    return;
  }

  Result match_result = GetAffiliationMatchVerificationResult(
      *affiliations_match_type, should_use_strong_matching);

  if (!ActorLoginFlowVerifier::IsSuccess(match_result)) {
    std::move(callback).Run(match_result);
    return;
  }

  // This consumes the context, which means that observing navigations in
  // the login frames also ends now.
  std::optional<autofill::ActorLoginContext> context =
      std::move(consume_context_callback).Run();
  if (!context.has_value()) {
    std::move(callback).Run(Result::kNoActorLoginContext);
    return;
  }

  if (!context->navigations_per_frame.contains(otp_frame_id)) {
    std::move(callback).Run(Result::kFrameNotInLoginContext);
    return;
  }

  // Actor Login filled credentials in all of these frames but the actual
  // login frame is unknown. While finding an OTP field in one
  // of those frames is a signal that the frame was the login frame, it's not
  // guaranteed. Therefore, require all frames to have <2 navigations to
  // avoid accidentally skipping user confirmation for OTPs not meant for
  // login flows.
  bool navigations_ok =
      std::ranges::all_of(context->navigations_per_frame,
                          [](const auto& entry) { return entry.second < 2; });
  if (!navigations_ok) {
    std::move(callback).Run(Result::kAllFramesHaveTooManyNavigations);
    return;
  }

  std::move(callback).Run(match_result);
}

std::ostream& operator<<(std::ostream& os,
                         ActorLoginFlowVerifier::Result result) {
  switch (result) {
    case Result::kExactMatchAllowed:
      return os << "ExactMatchAllowed";
    case Result::kAffiliatedMatchAllowed:
      return os << "AffiliatedMatchAllowed";
    case Result::kPslMatchAllowed:
      return os << "PslMatchAllowed";
    case Result::kNoActorLoginContext:
      return os << "NoActorLoginContext";
    case Result::kFrameNotInLoginContext:
      return os << "FrameNotInLoginContext";
    case Result::kAllFramesHaveTooManyNavigations:
      return os << "AllFramesHaveTooManyNavigations";
    case Result::kNoMatch:
      return os << "NoMatch";
    case Result::kPslMatchDisallowed:
      return os << "PslMatchDisallowed";
    case Result::kGroupedOrOtherMismatch:
      return os << "GroupedOrOtherMismatch";
    case Result::kMainFrameOriginMismatch:
      return os << "MainFrameOriginMismatch";
  }
  NOTREACHED();
}

}  // namespace actor
