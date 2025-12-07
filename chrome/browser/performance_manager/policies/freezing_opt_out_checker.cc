// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/freezing_opt_out_checker.h"

#include <string>
#include <string_view>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "url/gurl.h"

namespace performance_manager::policies {

FreezingOptOutChecker::FreezingOptOutChecker(
    base::WeakPtr<DiscardEligibilityPolicy> eligibility_policy)
    : eligibility_policy_(std::move(eligibility_policy)) {}

FreezingOptOutChecker::~FreezingOptOutChecker() = default;

void FreezingOptOutChecker::SetOptOutPolicyChangedCallback(
    OnPolicyChangedForBrowserContextCallback callback) {
  if (eligibility_policy_) {
    eligibility_policy_->SetOptOutPolicyChangedCallback(std::move(callback));
  }
}

bool FreezingOptOutChecker::IsPageOptedOutOfFreezing(
    std::string_view browser_context_id,
    const GURL& main_frame_url) {
  if (!eligibility_policy_) {
    // If DiscardEligibilityPolicy is deleted before FreezingOptOutChecker, the
    // opt-out policy is unavailable. Assume the page *could* be opted out.
    return true;
  }
  return eligibility_policy_->IsPageOptedOutOfDiscarding(
      std::string(browser_context_id), main_frame_url);
}

}  // namespace performance_manager::policies
