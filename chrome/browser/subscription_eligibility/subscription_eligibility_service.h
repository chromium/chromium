// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUBSCRIPTION_ELIGIBILITY_SUBSCRIPTION_ELIGIBILITY_SERVICE_H_
#define CHROME_BROWSER_SUBSCRIPTION_ELIGIBILITY_SUBSCRIPTION_ELIGIBILITY_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"

class PrefService;

namespace subscription_eligibility {

class SubscriptionEligibilityService : public KeyedService {
 public:
  explicit SubscriptionEligibilityService(PrefService* pref_service);
  SubscriptionEligibilityService(const SubscriptionEligibilityService&) =
      delete;
  SubscriptionEligibilityService operator=(
      const SubscriptionEligibilityService&) = delete;
  ~SubscriptionEligibilityService() override;

  // Returns the AI subscription tier for the user.
  int32_t GetAiSubscriptionTier();

 private:
  // Not owned. Guaranteed to outlive `this`.
  raw_ptr<PrefService> pref_service_;
};

}  // namespace subscription_eligibility

#endif  // CHROME_BROWSER_SUBSCRIPTION_ELIGIBILITY_SUBSCRIPTION_ELIGIBILITY_SERVICE_H_
