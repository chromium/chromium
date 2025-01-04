// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/freezing_opt_out_checker.h"

#include <string>
#include <string_view>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/performance_manager/policies/page_discarding_helper.h"
#include "url/gurl.h"

namespace performance_manager::policies {

FreezingOptOutChecker::FreezingOptOutChecker(
    base::WeakPtr<PageDiscardingHelper> discarding_helper)
    : discarding_helper_(std::move(discarding_helper)) {}

FreezingOptOutChecker::~FreezingOptOutChecker() = default;

void FreezingOptOutChecker::SetOptOutPolicyChangedCallback(
    OnPolicyChangedForBrowserContextCallback callback) {
  if (discarding_helper_) {
    discarding_helper_->SetOptOutPolicyChangedCallback(std::move(callback));
  }
}

bool FreezingOptOutChecker::IsPageOptedOutOfFreezing(
    std::string_view browser_context_id,
    const GURL& main_frame_url) {
  if (!discarding_helper_) {
    // If PageDiscardingHelper is deleted before FreezingOptOutChecker, the
    // opt-out policy is unavailable. Assume the page *could* be opted out.
    return true;
  }
  return discarding_helper_->IsPageOptedOutOfDiscarding(
      std::string(browser_context_id), main_frame_url);
}

}  // namespace performance_manager::policies
