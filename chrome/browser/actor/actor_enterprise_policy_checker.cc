// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_enterprise_policy_checker.h"

#include <utility>

#include "base/notimplemented.h"

namespace actor {

ActorEnterprisePolicyChecker::ActorEnterprisePolicyChecker(Profile* profile) {
  // TODO(crbug.com/568105613): Store profile and initialize
  // URLBlocklistManager from profile prefs.
  NOTIMPLEMENTED();
}

ActorEnterprisePolicyChecker::~ActorEnterprisePolicyChecker() = default;

EnterprisePolicyChecker::UrlBlockReason ActorEnterprisePolicyChecker::Evaluate(
    const GURL& url) const {
  // TODO(crbug.com/568105613): Implement enterprise URL policy evaluation.
  NOTIMPLEMENTED();
  return UrlBlockReason::kNotBlocked;
}

void ActorEnterprisePolicyChecker::ValidateContentSentToRenderer(
    content::RenderFrameHost* frame,
    const std::string& content,
    ContentValidationCallback callback) const {
  // TODO(crbug.com/568105613): Implement enterprise content validation.
  NOTIMPLEMENTED();
  std::move(callback).Run(ContentValidationReason::kAllowed);
}

}  // namespace actor
