// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/enterprise_policy_checker.h"

#include <utility>

#include "base/no_destructor.h"

namespace actor {

NullEnterprisePolicyChecker::NullEnterprisePolicyChecker() = default;
NullEnterprisePolicyChecker::~NullEnterprisePolicyChecker() = default;

EnterprisePolicyChecker::UrlBlockReason NullEnterprisePolicyChecker::Evaluate(
    const GURL& url) const {
  return UrlBlockReason::kNotBlocked;
}

void NullEnterprisePolicyChecker::ValidateContentSentToRenderer(
    content::RenderFrameHost* frame,
    const std::string& content,
    ContentValidationCallback callback) const {
  std::move(callback).Run(ContentValidationReason::kAllowed);
}

const EnterprisePolicyChecker* GetNullEnterprisePolicyChecker() {
  static base::NoDestructor<NullEnterprisePolicyChecker> checker;
  return checker.get();
}

}  // namespace actor
