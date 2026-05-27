// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_FREEZING_OPT_OUT_CHECKER_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_FREEZING_OPT_OUT_CHECKER_H_

#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "chrome/browser/performance_manager/policies/discard_eligibility_policy.h"
#include "components/performance_manager/public/freezing/freezing.h"

class GURL;

namespace performance_manager::policies {

class FreezingOptOutChecker final : public freezing::OptOutChecker {
 public:
  explicit FreezingOptOutChecker(
      base::WeakPtr<DiscardEligibilityPolicy> eligibility_policy);
  ~FreezingOptOutChecker() final;

  FreezingOptOutChecker(const FreezingOptOutChecker&) = delete;
  FreezingOptOutChecker& operator=(const FreezingOptOutChecker&) = delete;

  void SetOptOutPolicyChangedCallback(
      OnPolicyChangedForBrowserContextCallback callback) final;
  bool IsPageOptedOutOfFreezing(
      const base::UnguessableToken& browser_context_id,
      const GURL& main_frame_url) final;

 private:
  base::WeakPtr<DiscardEligibilityPolicy> eligibility_policy_;
};

}  // namespace performance_manager::policies

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_FREEZING_OPT_OUT_CHECKER_H_
